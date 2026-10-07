import { FILES } from "./files.ts";

const MIME_TYPES: Record<string, string> = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "application/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".gif": "image/gif",
  ".svg": "image/svg+xml",
  ".ico": "image/x-icon",
  ".woff": "font/woff",
  ".woff2": "font/woff2",
  ".ttf": "font/ttf",
  ".txt": "text/plain; charset=utf-8",
  ".xml": "application/xml; charset=utf-8",
  ".inv": "application/octet-stream",
};

function getMimeType(path: string): string {
  const ext = path.slice(path.lastIndexOf(".")).toLowerCase();
  return MIME_TYPES[ext] || "application/octet-stream";
}

Deno.serve((req: Request) => {
  const url = new URL(req.url);
  let pathname = url.pathname;

  if (pathname === "/" || pathname === "") {
    pathname = "/index.html";
  }

  if (pathname.includes("..")) {
    return new Response("403 Forbidden", { status: 403 });
  }

  if (
    pathname.startsWith("/zh/_static/") ||
    pathname.startsWith("/zh/_sphinx_design_static/")
  ) {
    pathname = pathname.replace("/zh/", "/en/");
  }

  const data = FILES[pathname];
  if (data) {
    return new Response(data, {
      headers: {
        "Content-Type": getMimeType(pathname),
        "Cache-Control": "public, max-age=3600",
      },
    });
  }

  const indexPath = pathname.endsWith("/")
    ? `${pathname}index.html`
    : `${pathname}/index.html`;
  const indexData = FILES[indexPath];
  if (indexData) {
    return new Response(indexData, {
      headers: {
        "Content-Type": "text/html; charset=utf-8",
        "Cache-Control": "public, max-age=3600",
      },
    });
  }

  return new Response("404 - Page not found", {
    status: 404,
    headers: { "Content-Type": "text/plain; charset=utf-8" },
  });
});
