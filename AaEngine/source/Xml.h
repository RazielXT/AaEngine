#pragma once

#include <vector>
#include <string>
#include <charconv>

namespace XmlParser
{
	struct Element
	{
		std::string_view name;
		enum class Type : uint8_t { Node, String, Attribute, Cdata } type;

		std::string_view value() const;
		std::string_view value(std::string_view name) const;
		bool hasValue = false;

		const Element* nextSibling() const;
		const Element* nextSibling(std::string_view name) const;
		uint32_t nextSiblingOffset = 0;

		const Element* firstNode() const;
		const Element* firstNode(std::string_view name) const;
		uint32_t firstChildOffset = 0;

		std::string_view attribute(std::string_view name) const;
		const Element* attributes() const;
		uint32_t attributesCount = 0;

		template <typename T>
		T attributeAs(std::string_view name, T defaultValue = {}) const
		{
			auto valueView = attribute(name);
			T value = defaultValue;
			std::from_chars(valueView.data(), valueView.data() + valueView.length(), value);
			return value;
		}

		struct iterator {
		public:
			iterator(const Element* ptr) : ptr(ptr) {}
			iterator operator++() { ptr = ptr->nextSibling(); return *this; }
			bool operator!=(const iterator& other) const { return ptr != other.ptr; }
			const Element& operator*() const { return *ptr; }
		private:
			const Element* ptr;
		};

		iterator begin() const { return { firstNode() }; };
		iterator end() const { return { nullptr }; };
	};

	struct Document
	{
		const Element* getRoot();

		bool load(const char* filename);
		bool parse(const char* input, size_t size);

	private:
		std::vector<Element> elements;
		std::string dataHolder;
	};
};
