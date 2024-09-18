#include "Xml.h"
#include <algorithm>
#include <fstream>

const XmlParser::Element* XmlParser::Document::getRoot()
{
	return elements.empty() ? nullptr : &elements.front();
}

bool XmlParser::Document::load(const char* filename)
{
	std::ifstream t(filename);
	dataHolder = std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

	return parse(dataHolder.c_str(), dataHolder.size());
}

bool XmlParser::Document::parse(const char* input, size_t size)
{
	elements.reserve(std::min((size_t)100, size / 25));

	char const* p = input;
	char const* end = input + size;

	std::vector<size_t> currentTreeStack;
	size_t prevSibling = 0;

	for (; p != end; ++p)
	{
		char const* start = p;
		for (; p != end && *p != '<'; ++p);

		if (p != start)
		{
			if (!currentTreeStack.empty())
			{
				elements[currentTreeStack.back()].hasValue = true;
			}
		}

		if (p == end) break;

		// '<'
		++p;
		if (p != end && p + 8 < end && std::memcmp(p, "![CDATA[", 8) == 0)
		{
			p += 8;
			start = p;
			while (p != end && std::memcmp(p - 2, "]]>", 3) != 0) ++p;

			// parse error
			if (p == end)
			{
				return false;
			}

			elements.push_back({ { start, std::size_t(p - start - 2) }, Element::Type::Cdata });
			continue;
		}

		// parse the name of the tag.
		for (start = p; p != end && *p != '>' && !isspace(*p); ++p);

		char const* tag_name_end = p;

		// skip the attributes for now
		for (; p != end && *p != '>'; ++p);

		// parse error
		if (p == end)
		{
			return false;
		}

		size_t currentNode = 0;

		char const* tag_end = p;
		if (*start == '/')
		{
			++start;
			if (!currentTreeStack.empty())
			{
				prevSibling = currentTreeStack.back();
				currentTreeStack.pop_back();
			}
		}
		else if (*start == '?' && *(p - 1) == '?')
		{
			++start;
			//declaration = { start, std::size_t(std::min(tag_name_end - start, p - start - 1)) };
			tag_end = p - 1;
		}
		else if (start + 5 < p && std::memcmp(start, "!--", 3) == 0 && std::memcmp(p - 2, "--", 2) == 0)
		{
			start += 3;
			continue;
		}
		else //add new node
		{
			//sibling
			if (prevSibling)
			{
				elements[prevSibling].nextSiblingOffset = (uint32_t)(elements.size() - prevSibling);
				prevSibling = 0;
			}
			//parent
			if (!currentTreeStack.empty() && elements[currentTreeStack.back()].firstChildOffset == 0)
			{
				elements[currentTreeStack.back()].firstChildOffset = (uint32_t)(elements.size() - currentTreeStack.back());
			}

			if (*(p - 1) == '/')
			{
				prevSibling = elements.size();
				tag_end = p - 1;
			}
			else
				currentTreeStack.push_back(elements.size());

			currentNode = elements.size();
			elements.push_back({ { start, std::size_t(tag_name_end - start) }, Element::Type::Node });
		}

		// parse attributes
		for (char const* i = tag_name_end; i < tag_end; ++i)
		{
			char const* val_start = nullptr;

			// find start of attribute name
			while (i != tag_end && isspace(*i)) ++i;
			if (i == tag_end) break;
			start = i;
			// find end of attribute name
			while (i != tag_end && *i != '=' && !isspace(*i)) ++i;
			auto const name_len = std::size_t(i - start);

			// look for equality sign
			for (; i != tag_end && *i != '='; ++i);

			// no equality sign found
			if (i == tag_end)
			{
				break;
			}

			++i;
			while (i != tag_end && isspace(*i)) ++i;
			// check for parse error (values must be quoted)
			if (i == tag_end || (*i != '\'' && *i != '\"'))
			{
				return false;
			}
			char quote = *i;
			++i;
			val_start = i;
			for (; i != tag_end && *i != quote; ++i);
			// parse error (missing end quote)
			if (i == tag_end)
			{
				return false;
			}

			if (currentNode)
			{
				elements.push_back({ { start, name_len }, Element::Type::Attribute, true, 2 });
				elements.push_back({ { val_start, std::size_t(i - val_start) }, Element::Type::String });

				elements[currentNode].attributesCount++;
			}
		}

		if (currentNode && elements[currentNode].attributesCount)
		{
			elements[elements.size() - 2].nextSiblingOffset = 0;
		}
	}

	return !elements.empty() && currentTreeStack.empty();
}

std::string_view XmlParser::Element::value() const
{
	auto valuePtr = this + attributesCount + 1;

	return (hasValue && valuePtr->type == Element::Type::String) || valuePtr->type == Element::Type::Cdata ? valuePtr->name : "";
}

std::string_view XmlParser::Element::value(std::string_view name) const
{
	if (auto node = firstNode(name))
		return node->value();

	return "";
}

std::string_view XmlParser::Element::attribute(std::string_view name) const
{
	if (auto a = attributes())
	{
		while (a)
		{
			if (a->name == name)
				return a->value();

			a = a->nextSibling();
		}
	}

	return "";
}

const XmlParser::Element* XmlParser::Element::attributes() const
{
	return attributesCount ? this + 1 : nullptr;
}

const XmlParser::Element* XmlParser::Element::nextSibling() const
{
	return nextSiblingOffset ? this + nextSiblingOffset : nullptr;
}


const XmlParser::Element* XmlParser::Element::nextSibling(std::string_view name) const
{
	auto node = nextSibling();

	while (node)
	{
		if (node->name == name)
			break;

		node = node->nextSibling();
	}

	return node;
}

const XmlParser::Element* XmlParser::Element::firstNode() const
{
	return firstChildOffset ? this + firstChildOffset : nullptr;
}

const XmlParser::Element* XmlParser::Element::firstNode(std::string_view name) const
{
	auto node = firstNode();

	while (node)
	{
		if (node->name == name)
			break;

		node = node->nextSibling();
	}

	return node;
}
