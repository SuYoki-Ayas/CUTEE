#pragma once

#include <QString>

#include <optional>
#include <utility>

namespace cutee {

/// Lightweight error payload shared by every fallible core API.
class Error
{
public:
    Error() = default;
    explicit Error(QString message, QString context = {})
        : m_message(std::move(message))
        , m_context(std::move(context))
    {
    }

    const QString &message() const { return m_message; }
    const QString &context() const { return m_context; }
    bool isEmpty() const { return m_message.isEmpty() && m_context.isEmpty(); }

    QString toString() const
    {
        if (m_context.isEmpty())
            return m_message;
        if (m_message.isEmpty())
            return m_context;
        return QStringLiteral("%1: %2").arg(m_context, m_message);
    }

private:
    QString m_message;
    QString m_context;
};

/// Minimal `expected`-like type; C++17 has no std::expected.
template <typename T>
class Result
{
public:
    Result(T value) // NOLINT(google-explicit-constructor): mirrors expected semantics
        : m_value(std::move(value))
    {
    }
    Result(Error error) // NOLINT(google-explicit-constructor)
        : m_error(std::move(error))
    {
    }

    bool ok() const { return !m_error.has_value(); }
    explicit operator bool() const { return ok(); }

    const T &value() const { return *m_value; }
    T &value() { return *m_value; }

    const Error &error() const { return *m_error; }
    QString errorMessage() const { return m_error ? m_error->toString() : QString(); }

private:
    std::optional<T> m_value;
    std::optional<Error> m_error;
};

template <>
class Result<void>
{
public:
    Result() = default;
    Result(Error error) // NOLINT(google-explicit-constructor)
        : m_error(std::move(error))
    {
    }

    bool ok() const { return !m_error.has_value(); }
    explicit operator bool() const { return ok(); }

    const Error &error() const { return *m_error; }
    QString errorMessage() const { return m_error ? m_error->toString() : QString(); }

private:
    std::optional<Error> m_error;
};

} // namespace cutee
