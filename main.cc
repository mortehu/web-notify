#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "crow.h"

// Must be included after "crow.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>

int main() {
  PangoFontMap* fontMap = pango_cairo_font_map_get_default();
  PangoContext* pangoContext = pango_font_map_create_context(fontMap);
  PangoFontDescription* fontDescription = pango_font_description_from_string(
      "Noto Color Emoji 100px, DejaVu Serif 100px");

  auto display = XOpenDisplay(NULL);
  if (display == NULL) {
    std::cerr << "Cannot open display" << std::endl;
    exit(1);
  }

  auto xScreen = DefaultScreen(display);
  auto rootWindow = RootWindow(display, xScreen);

  crow::SimpleApp app;

  CROW_ROUTE(app, "/notify")
      .methods("GET"_method)([&](const crow::request& req) {
        auto message = req.url_params.get("message");
        if (!message) return crow::response(400);

        // Render text.
        PangoLayout* layout = pango_layout_new(pangoContext);
        pango_layout_set_font_description(layout, fontDescription);

        pango_layout_set_text(layout, message, -1);

        int width, height;
        pango_layout_get_pixel_size(layout, &width, &height);

        auto pixels = std::make_unique<unsigned char[]>(width * height * 4);
        cairo_surface_t* cairoSurface = cairo_image_surface_create_for_data(
            pixels.get(), CAIRO_FORMAT_ARGB32, width, height, width * 4);
        cairo_t* cairo = cairo_create(cairoSurface);

        cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);

        pango_cairo_update_layout(cairo, layout);
        pango_cairo_show_layout(cairo, layout);

        cairo_surface_destroy(cairoSurface);
        cairo_destroy(cairo);

        // Display the message in an override-redirect window.
        XSetWindowAttributes attributes;
        attributes.override_redirect = True;
        attributes.background_pixel = BlackPixel(display, xScreen);
        attributes.border_pixel = BlackPixel(display, xScreen);
        attributes.event_mask = ExposureMask | KeyPressMask | PointerMotionMask;

        std::vector<XineramaScreenInfo> screens;
        int dummy;

        if (XineramaQueryExtension(display, &dummy, &dummy) &&
            XineramaIsActive(display)) {
          int screenCount;
          auto tmpScreens = XineramaQueryScreens(display, &screenCount);

          screens.assign(tmpScreens, tmpScreens + screenCount);

          XFree(tmpScreens);
        } else {
          // Xinerama not active -> assume one screen.
          XWindowAttributes rootWindowAttr;
          XGetWindowAttributes(display, rootWindow, &rootWindowAttr);

          XineramaScreenInfo tmp;
          tmp.x_org = 0;
          tmp.y_org = 0;
          tmp.width = rootWindowAttr.width;
          tmp.height = rootWindowAttr.height;

          screens.emplace_back(tmp);
        }

        auto image = XCreateImage(display, DefaultVisual(display, xScreen),
                                  DefaultDepth(display, xScreen), ZPixmap, 0,
                                  reinterpret_cast<char*>(pixels.get()), width,
                                  height, 32, 0);
        image->f.destroy_image = +[](XImage*) { return 0; };

        std::unordered_map<Window, GC> windows;

        for (const auto& screen : screens) {
          auto window = XCreateWindow(
              display, rootWindow, screen.x_org + screen.width / 2 - width / 2,
              screen.y_org + screen.height / 2 - height / 2, width, height, 1,
              DefaultDepth(display, xScreen), InputOutput,
              DefaultVisual(display, xScreen),
              CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
              &attributes);

          auto gc = XCreateGC(display, window, 0, NULL);

          XPutImage(display, window, gc, image, 0, 0, 0, 0, width, height);

          XMapWindow(display, window);

          windows.emplace(window, gc);
        }

        auto displayUntil =
            std::chrono::system_clock::now() + std::chrono::milliseconds(500);

        XGrabKeyboard(display, rootWindow, True, GrabModeAsync, GrabModeAsync,
                      CurrentTime);
        XGrabPointer(display, rootWindow, True, PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

        // 8. Handle events and clean up when done
        XEvent event;
        bool running = true;
        while (running) {
          XNextEvent(display, &event);
          switch (event.type) {
            case Expose:
              if (auto it = windows.find(event.xexpose.window);
                  it != windows.end()) {
                XPutImage(display, event.xexpose.window, it->second, image, 0,
                          0, 0, 0, width, height);
              }
              break;
            case KeyPress:
              running = false;
              break;
            case MotionNotify:
              running = false;
              break;
          }
        }

        XUngrabPointer(display, CurrentTime);
        XUngrabKeyboard(display, CurrentTime);

        XDestroyImage(image);

        std::this_thread::sleep_until(displayUntil);

        for (const auto& it : windows) XDestroyWindow(display, it.first);

        XSync(display, False);

        return crow::response(200);
      });

  app.port(19324).multithreaded().run();

  XCloseDisplay(display);

  return 0;
}
