#pragma once

#include "../std.hpp"

namespace cortono::http
{
    using namespace std::literals;

    enum class ResponseStatus
    {
        OK,
        BadRequest
    };

    enum class ResponseHeader
    {
        Connection,
        Content_Length,
        Content_Type,
        Last_Modified_Time,
        Content_Encoding,
        Transfer_Encoding
    };

    constexpr inline auto conv_header_to_string(
        std::integral_constant<ResponseHeader,
                               ResponseHeader::Content_Length>) {
        return "Content-Length"sv;
    }
    constexpr inline auto conv_header_to_string(
        std::integral_constant<ResponseHeader,
                               ResponseHeader::Content_Type>) {
        return "Content-Type"sv;
    }

    inline std::string_view ResponseHeader_to_sv(ResponseHeader header) {
        switch(header) {
            case ResponseHeader::Content_Length:
                return "Content-Length"sv;
            case ResponseHeader::Content_Type:
                return "Content-Type"sv;
            case ResponseHeader::Last_Modified_Time:
                return "Last-Modified-Time"sv;
            case ResponseHeader::Connection:
                return "Connection"sv;
            case ResponseHeader::Content_Encoding:
                return "Content-Encoding";
            case ResponseHeader::Transfer_Encoding:
                return "Transfer-Encoding";
        }
    }

	constexpr inline std::string_view switching_protocols = "HTTP/1.1 101 Switching Protocals\r\n"sv;
	constexpr inline std::string_view rep_ok = "HTTP/1.1 200 OK\r\n"sv;
	constexpr inline std::string_view rep_created = "HTTP/1.1 201 Created\r\n"sv;
	constexpr inline std::string_view rep_accepted = "HTTP/1.1 202 Accepted\r\n"sv;
	constexpr inline std::string_view rep_no_content = "HTTP/1.1 204 No Content\r\n"sv;
	constexpr inline std::string_view rep_partial_content = "HTTP/1.1 206 Partial Content\r\n"sv;
	constexpr inline std::string_view rep_multiple_choices = "HTTP/1.1 300 Multiple Choices\r\n"sv;
	constexpr inline std::string_view rep_moved_permanently =	"HTTP/1.1 301 Moved Permanently\r\n"sv;
	constexpr inline std::string_view rep_moved_temporarily =	"HTTP/1.1 302 Moved Temporarily\r\n"sv;
	constexpr inline std::string_view rep_not_modified = "HTTP/1.1 304 Not Modified\r\n"sv;
	constexpr inline std::string_view rep_bad_request = "HTTP/1.1 400 Bad Request\r\n"sv;
	constexpr inline std::string_view rep_unauthorized = "HTTP/1.1 401 Unauthorized\r\n"sv;
	constexpr inline std::string_view rep_forbidden =	"HTTP/1.1 403 Forbidden\r\n"sv;
	constexpr inline std::string_view rep_not_found =	"HTTP/1.1 404 Not Found\r\n"sv;
	constexpr inline std::string_view rep_internal_server_error = "HTTP/1.1 500 Internal Server Error\r\n"sv;
	constexpr inline std::string_view rep_not_implemented = "HTTP/1.1 501 Not Implemented\r\n"sv;
	constexpr inline std::string_view rep_bad_gateway = "HTTP/1.1 502 Bad Gateway\r\n"sv;
	constexpr inline std::string_view rep_service_unavailable = "HTTP/1.1 503 Service Unavailable\r\n"sv;

    std::string_view status_to_sv(ResponseStatus status) {
        switch(status) {
            case ResponseStatus::OK:
                return rep_ok;
            case ResponseStatus::BadRequest:
                return rep_bad_request;
            default:
                return {};
        }
    }

	inline std::string_view ok = "OK";
	inline std::string_view created = "<html>"
		"<head><title>Created</title></head>"
		"<body><h1>201 Created</h1></body>"
		"</html>";

	inline std::string_view accepted =
		"<html>"
		"<head><title>Accepted</title></head>"
		"<body><h1>202 Accepted</h1></body>"
		"</html>";

	inline std::string_view no_content =
		"<html>"
		"<head><title>No Content</title></head>"
		"<body><h1>204 Content</h1></body>"
		"</html>";

	inline std::string_view multiple_choices =
		"<html>"
		"<head><title>Multiple Choices</title></head>"
		"<body><h1>300 Multiple Choices</h1></body>"
		"</html>";

	inline std::string_view moved_permanently =
		"<html>"
		"<head><title>Moved Permanently</title></head>"
		"<body><h1>301 Moved Permanently</h1></body>"
		"</html>";

	inline std::string_view moved_temporarily =
		"<html>"
		"<head><title>Moved Temporarily</title></head>"
		"<body><h1>302 Moved Temporarily</h1></body>"
		"</html>";

	inline std::string_view not_modified =
		"<html>"
		"<head><title>Not Modified</title></head>"
		"<body><h1>304 Not Modified</h1></body>"
		"</html>";

	inline std::string_view bad_request =
		"<html>"
		"<head><title>Bad Request</title></head>"
		"<body><h1>400 Bad Request</h1></body>"
		"</html>";

	inline std::string_view unauthorized =
		"<html>"
		"<head><title>Unauthorized</title></head>"
		"<body><h1>401 Unauthorized</h1></body>"
		"</html>";

	inline std::string_view forbidden =
		"<html>"
		"<head><title>Forbidden</title></head>"
		"<body><h1>403 Forbidden</h1></body>"
		"</html>";

	inline std::string_view not_found =
		"<html>"
		"<head><title>Not Found</title></head>"
		"<body><h1>404 Not Found</h1></body>"
		"</html>";

	inline std::string_view internal_server_error =
		"<html>"
		"<head><title>Internal Server Error</title></head>"
		"<body><h1>500 Internal Server Error</h1></body>"
		"</html>";

	inline std::string_view not_implemented =
		"<html>"
		"<head><title>Not Implemented</title></head>"
		"<body><h1>501 Not Implemented</h1></body>"
		"</html>";

	inline std::string_view bad_gateway =
		"<html>"
		"<head><title>Bad Gateway</title></head>"
		"<body><h1>502 Bad Gateway</h1></body>"
		"</html>";

	inline std::string_view service_unavailable =
		"<html>"
		"<head><title>Service Unavailable</title></head>"
		"<body><h1>503 Service Unavailable</h1></body>"
		"</html>";

    std::string_view status_to_content(ResponseStatus status) {
        switch(status) {
            case ResponseStatus::OK:
                return ok;
            case ResponseStatus::BadRequest:
                return bad_request;
            default:
                return {};
        }
    }

    constexpr inline auto key_value_spacer = ": "sv;
    constexpr inline auto crlf = "\r\n";
}
