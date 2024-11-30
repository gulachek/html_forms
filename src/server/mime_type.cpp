#include "mime_type.hpp"

std::string_view mime_type(const std::string_view &ext) {
#define MAP(EXT, MIME)                                                         \
  if (ext == EXT)                                                              \
    return MIME;

  // text
  MAP("htm", "text/html")
  MAP("html", "text/html")
  MAP("html", "text/html")
  MAP("css", "text/css")
  MAP("txt", "text/plain")
  MAP("js", "text/javascript")
  MAP("mjs", "text/javascript")
  MAP("json", "application/json")
  MAP("xml", "application/xml")

  // image
  MAP("png", "image/png")
  MAP("jpe", "image/jpeg")
  MAP("jpeg", "image/jpeg")
  MAP("jpg", "image/jpeg")
  MAP("jif", "image/jpeg")
  MAP("jfif", "image/jpeg")
  MAP("jfi", "image/jpeg")
  MAP("gif", "image/gif")
  MAP("bmp", "image/bmp")
  MAP("dib", "image/bmp")
  MAP("ico", "image/vnd.microsoft.icon")
  MAP("tiff", "image/tiff")
  MAP("tif", "image/tiff")
  MAP("svg", "image/svg+xml")
  MAP("svgz", "image/svg+xml")
  MAP("webp", "image/webp")
  MAP("avif", "image/avif")
  // TODO - heif/heic

  // font
  MAP("otf", "font/otf")
  MAP("ttf", "font/ttf")
  MAP("woff", "font/woff")
  MAP("woff2", "font/woff2")
  MAP("eot", "application/vnd.ms-fontobject")

  // audio
  MAP("mp3", "audio/mpeg")
  MAP("wav", "audio/wav")
  MAP("weba", "audio/webm")
  MAP("mid", "audio/midi")
  MAP("midi", "audio/midi")
  MAP("oga", "audio/ogg")
  MAP("opus", "audio/opus")

  // video
  MAP("mp4", "video/mp4")
  MAP("mpeg", "video/mpeg")
  MAP("webm", "video/webm")
  MAP("avi", "video/x-msvideo")

#undef MAP

  return "text/plain";
}
