#include "json_builder.h"


namespace mcptoolkit {

	JsonBuilder::JsonBuilder() {
		buffer.reserve(4096);
	}
	void JsonBuilder::start_object() {
		buffer += '{';
	}
	void JsonBuilder::end_object() {
		buffer += '}';
	}
    void JsonBuilder::add_field(const char* key, const char* value) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":\"";
        append_escaped(buffer, value);
        buffer += '"';
    }

    void JsonBuilder::add_field_number(const char* key, int value) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":";
        buffer += std::to_string(value);
    }

    void JsonBuilder::add_field_double(const char* key, double value) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":";
        char buf[32];
        snprintf(buf, sizeof(buf), "%.15g", value);
        buffer += buf;
    }

    void JsonBuilder::add_field_bool(const char* key, bool value) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":";
        buffer += value ? "true" : "false";
    }

    void JsonBuilder::add_field_raw(const char* key, const char* value) {
        if (!buffer.empty() && buffer.back() != '{' && buffer.back() != '[') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":";
        buffer += value;
    }

    void JsonBuilder::add_object_field(const char* key) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":{";
    }

    void JsonBuilder::close_object_field() {
        buffer += '}';
    }

    void JsonBuilder::add_array_field(const char* key) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":[";
    }

    void JsonBuilder::close_array_field() {
        buffer += ']';
    }

    void JsonBuilder::add_array_element(const char* value) {
        if (!buffer.empty() && buffer.back() != '[') buffer += ',';
        buffer += '"';
        append_escaped(buffer, value);
        buffer += '"';
    }

    void JsonBuilder::add_array_element_raw(const char* value) {
        if (!buffer.empty() && buffer.back() != '[') buffer += ',';
        buffer += value;
    }

    const std::string& JsonBuilder::get() const {
        return buffer;
    }

    void JsonBuilder::reset() {
        buffer.clear();
    }

    const char* JsonBuilder::c_str() const {
        return buffer.c_str();
    }

    size_t JsonBuilder::size() const {
        return buffer.size();
    }
}