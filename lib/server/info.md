# lib/server

`server.h` contains implementation-independent request, response, method, and callback types. `server/mongoose/mngs.h/.cpp` is a thin C++ wrapper over Mongoose 7.22 for an HTTP + WebSocket server on one listener and one internal event-loop thread.

## lib/server/mongoose

### `std::thread startServer(uint32_t ipV4Host, uint16_t port)`

Starts the Mongoose manager and HTTP listener in an internal thread. `ipV4Host` is IPv4 in host byte order. The function returns after the listener has either started or failed. The network thread uses blocking `mg_mgr_poll()` and does not busy-poll.

### `void stopServer()`

Requests the event-loop thread to stop and joins it. If the server is not running, does nothing.

## HTTP

### `void addServerHTTPHandler(HTTP_METHOD method, const std::string& route, ServerHTTPHandler callback)`

Registers an exact-path HTTP handler. Handlers must be registered before `startServer()`. The callback receives a copied `ServerRequest` (`method`, `uri`, `body`) and fills `ServerResponse` (`status`, `contentType`, `body`). Unknown routes return 404.

## WebSocket

### `void addServerWebSocketHandler(const std::string& route, ServerWebSocketHandler callback)`

Registers an exact-path WebSocket endpoint before server start. Each text message is passed to the callback. If the callback writes a non-empty response string, it is sent back as a text WebSocket frame.

`server/mongoose/_example/hello.cpp` demonstrates both a GET endpoint and `/ws` WebSocket echo-style handling on the same port. `mongoose/mngs_test.cpp` uses GTest and a Mongoose client manager to verify both HTTP and WebSocket paths.

Mongoose is pinned to tag `7.22` by top-level CMake FetchContent and is compiled as C++ because the top-level project enables only the CXX language.


## lib/server/cpp-httplib

`server/cpp-httplib/http.h/.cpp` — тонка обгортка над cpp-httplib для простого HTTP-сервера.

### `void startServerHTTP(uint32_t ipV4Host, uint16_t port)`

Прив'язує сервер до IPv4-адреси у host byte order і запускає `listen_after_bind()` у внутрішньому потоці. Функція повертається після успішного bind. Помилки друкуються в stderr з префіксом `on startServerHTTP():`.

### `void stopServerHTTP()`

Зупиняє сервер і очікує завершення внутрішнього потоку. Якщо сервер не запущено, нічого не робить.

### `void addHandler(HTTP_METHOD method, const std::string& route, HTTPHandler callback)`

Реєструє callback для маршруту. `HTTP_METHOD` підтримує `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `OPTIONS`, `HEAD`. Callback має сигнатуру `void(const httplib::Request&, httplib::Response&)`. Для `HEAD` використовується GET-route cpp-httplib, який автоматично формує HEAD-відповідь без body.

`server/cpp-httplib/_example/hello.cpp` містить мінімальний text/plain GET-приклад. `server/cpp-httplib/http_test.cpp` стартує loopback-сервер і клієнт, виконує GET та перевіряє status і ключовий рядок у body; `../test_run.cpp` є пускачем сценарію.

