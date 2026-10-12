#pragma once

#include <string>
#include <utility>
#include <variant>

namespace g2n {

struct Error {
    std::string message;
};

/// Minimal expected-like type (std::expected is C++23). Errors are values:
/// parsing and I/O never throw across module boundaries.
template <class T>
class Result {
public:
    Result(T value) : storage_(std::move(value)) {}     // NOLINT(google-explicit-constructor)
    Result(Error error) : storage_(std::move(error)) {} // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool ok() const { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const { return ok(); }

    [[nodiscard]] T& value() & { return std::get<T>(storage_); }
    [[nodiscard]] const T& value() const& { return std::get<T>(storage_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(storage_)); }
    [[nodiscard]] const Error& error() const { return std::get<Error>(storage_); }

private:
    std::variant<T, Error> storage_;
};

[[nodiscard]] inline Error make_error(std::string message) {
    return Error{std::move(message)};
}

} // namespace g2n
