// SPDX-License-Identifier: MIT
/*
 * Digital Voice Modem - Common Library
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018 Jimmie Bergmann
 *  Copyright (C) 2020 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup yaml YAML
 * @brief Defines and implements the YAML file processor.
 * @ingroup common
 * 
 * @file Yaml.h
 * @ingroup yaml
 * @file Yaml.cpp
 * @ingroup yaml
 */
#if !defined(__YAML_H__)
#define __YAML_H__

/**
* YAML documentation:
* http://yaml.org/spec/1.0/index.html
* https://www.codeproject.com/Articles/28720/YAML-Parser-in-C
*/

#include "common/Defines.h"

#include <exception>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>

namespace yaml
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API Node;

    // ---------------------------------------------------------------------------
    //  Helper classes and functions
    // ---------------------------------------------------------------------------
    namespace impl
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief StringConverter Helper to converting string to any data type. Strings are left untouched.
         * @tparam T Atomic type to convert.
         */
        template<typename T>
        struct StringConverter {
            /**
             * @brief Return atomic type for given string.
             * @param data String to convert to atomic type.
             * @return T Atomic type value.
             */
            static T get(const std::string& data)
            {
                T type;
                std::stringstream ss(data);
                ss >> type;
                return type;
            }

            /**
             * @brief Return atomic type for a given string, with a fallback default.
             * @param data String to convert to atomic type.
             * @param defaultValue Default atomic type value.
             * @return T Atomic type value.
             */
            static T get(const std::string& data, const T& defaultValue)
            {
                T type;
                std::stringstream ss(data);
                ss >> type;

                if (ss.fail()) {
                    return defaultValue;
                }

                return type;
            }
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief StringConverter<std::string> Helper to convert a string to a string.
         * (This is just a default converter.)
         */
        template<>
        struct StringConverter<std::string> {
            /**
             * @brief Return string for the given string.
             * @param data String.
             * @return std::string String.
             */
            static std::string get(const std::string& data)
            {
                return data;
            }

            /**
             * @brief Return a string for the given string with a fallback default.
             * @param data String.
             * @param defaultValue Default string.
             * @return std::string String.
             */
            static std::string get(const std::string& data, const std::string& defaultValue)
            {
                if (data.size() == 0) {
                    return defaultValue;
                }
                return data;
            }
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief StringConverter<bool> Helper to convert a boolean to a string.
         * (This is just a default converter.)
         */
        template<>
        struct StringConverter<bool> {
            /**
             * @brief Return string for the given boolean.
             * @param data String.
             * @return bool Boolean value.
             */
            static bool get(const std::string& data)
            {
                std::string tmpData = data;
                std::transform(tmpData.begin(), tmpData.end(), tmpData.begin(), ::tolower);
                if (tmpData == "true" || tmpData == "yes" || tmpData == "1") {
                    return true;
                }

                return false;
            }

            /**
             * @brief Return a string for the given boolean with a fallback default.
             * @param data String.
             * @param defaultValue Default boolean.
             * @return bool Boolean value.
             */
            static bool get(const std::string& data, const bool& defaultValue)
            {
                if (data.size() == 0) {
                    return defaultValue;
                }

                return get(data);
            }
        };
    } // namespace impl

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Exception General YAML Exception.
     * @ingroup yaml
     */
    class HOST_SW_API Exception : public std::runtime_error {
    public:
        /**
         * @brief Enumeration of exception types.
         */
        enum eType
        {
            InternalError,  // Internal error.
            ParsingError,   // Invalid parsing data.
            OperationError  // User operation error.
        };

        /**
         * @brief Initializes a new instance of the Exception class.
         * @param message Exception message.
         * @param type Exception type.
         */
        Exception(const std::string & message, const eType type);

        /**
         * @brief Get type of exception.
         * @returns Type of exception.
         */
        eType type() const;

        /**
         * @brief Get message of exception.
         * @returns Exception message.
         */
        const char* message() const;

    private:
        eType m_Type;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief InternalException YAML Internal Exception.
     * @ingroup yaml
     */
    class HOST_SW_API InternalException : public Exception {
    public:
        /**
         * @brief Initializes a new instance of the InternalException class.
         * @param message Exception message.
         */
        InternalException(const std::string& message);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief ParsingException YAML Parsing Exception.
     * @ingroup yaml
     */
    class HOST_SW_API ParsingException : public Exception {
    public:
        /**
         * @brief Initializes a new instance of the ParsingException class.
         * @param message Exception message.
         */
        ParsingException(const std::string& message);

    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief OperationException YAML Operation Exception.
     * @ingroup yaml
     */
    class HOST_SW_API OperationException : public Exception {
    public:
        /**
         * @brief Initializes a new instance of the OperationException class.
         * @param message Exception message.
         */
        OperationException(const std::string & message);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Iterator Helper class used to iterate through YAML files.
     * @ingroup yaml
     */
    class HOST_SW_API Iterator {
    public:
        friend class Node;

        /**
         * @brief Initializes a new instance of the Iterator class.
         */
        Iterator();
        /**
         * @brief Copies an instance of the Iterator class to a new instance of the Iterator class.
         * @param it Iterator instance to copy from.
         */
        Iterator(const Iterator& it);
        /**
         * @brief Finalizes a instance of the Iterator class.
         */
        ~Iterator();

        /**
         * @brief Assignment operator.
         * @param it Iterator instance to copy from.
         */
        Iterator& operator= (const Iterator& it);

        /**
         * @brief Get node of iterator. First pair item is the key of map value, empty if type is sequence.
         */
        std::pair<const std::string&, Node&> operator* ();

        /**
         * @brief Post-increment operator.
         */
        Iterator& operator++ (int);
        /**
         * @brief Post-decrement operator.
         */
        Iterator& operator-- (int);

        /**
         * @brief Check if iterator is equal to other iterator.
         * @param it Iterator to check equality with.
         */
        bool operator== (const Iterator& it);

        /**
         * @brief Check if iterator is not equal to other iterator.
         * @param it Iterator to check inequality with.
         */
        bool operator!= (const Iterator& it);

    private:
        enum eType
        {
            None,
            SequenceType,
            MapType
        };

        eType m_Type;
        void* m_pImp;

    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief ConstIterator Helper class used to iterate through YAML files.
     * @ingroup yaml
     */
    class HOST_SW_API ConstIterator {
    public:
        friend class Node;

        /**
         * @brief Initializes a new instance of the ConstIterator class.
         */
        ConstIterator();
        /**
         * @brief Copies an instance of the ConstIterator class to a new instance of the ConstIterator class.
         * @param it Iterator instance to copy from.
         */
        ConstIterator(const ConstIterator& it);
        /**
         * @brief Finalizes a instance of the ConstIterator class.
         */
        ~ConstIterator();

        /**
         * @brief Assignment operator.
         * @param it Iterator instance to copy from.
         */
        ConstIterator& operator= (const ConstIterator& it);

        /**
         * @brief Get node of iterator. First pair item is the key of map value, empty if type is sequence.
         */
        std::pair<const std::string&, const Node&> operator *();

        /**
         * @brief Post-increment operator.
         */
        ConstIterator& operator++ (int);
        /**
         * @brief Post-decrement operator.
         */
        ConstIterator& operator-- (int);

        /**
         * @brief Check if iterator is equal to other iterator.
         * @param it Iterator to check equality with.
         */
        bool operator== (const ConstIterator& it);

        /**
         * @brief Check if iterator is not equal to other iterator.
         * @param it Iterator to check inequality with.
         */
        bool operator!= (const ConstIterator& it);

    private:
        enum eType
        {
            None,
            SequenceType,
            MapType
        };

        eType m_Type;
        void* m_pImp;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Node Represents a node/element within a YAML file.
     * @ingroup yaml
     */
    class HOST_SW_API Node {
    public:
        friend class Iterator;

        /**
         * @brief Enumeration of node types.
         */
        enum eType
        {
            None,
            SequenceType,
            MapType,
            ScalarType
        };

        /**
         * @brief Initializes a new instance of the Node class.
         */
        Node();
        /**
         * @brief Copies an instance of the Node class to a new instance of the Node class.
         * @param node Node instance to copy from.
         */
        Node(const Node& node);
        /**
         * @brief Initializes a new instance of the Node class.
         * @param value String value to initialize node with.
         */
        Node(const std::string& value);
        /**
         * @brief Initializes a new instance of the Node class.
         * @param value String value to initialize node with.
         */
        Node(const char* value);
        /**
         * @brief Finalizes a instance of the Node class.
         */
        ~Node();

        /**
         * @brief Gets the type of node.
         * @returns eType Node type.
         */
        eType type() const;
        /**
         * @brief Checks if the node contains nothing.
         * @returns bool True, if node is empty, otherwise false.
         */
        bool isNone() const;
        /**
         * @brief Checks if the node is a sequence node.
         * @returns bool True, if node is a sequence node, otherwise false.
         */
        bool isSequence() const;
        /**
         * @brief Checks if the node is a map node.
         * @returns bool True, if node is a map node, otherwise false.
         */
        bool isMap() const;
        /**
         * @brief Checks if the node is a scalar node.
         * @returns bool True, if node is a scalar node, otherwise false.
         */
        bool isScalar() const;

        /**
         * @brief Completely clear node.
         */
        void clear();

        /**
         * @brief Get node as given template type.
         * @tparam T Atomic Type.
         * @returns T Node converted to specified atomic type.
         */
        template<typename T>
        T as() const
        {
            return impl::StringConverter<T>::get(asString());
        }
        /**
         * @brief Get node as given template type with a default value if no value is found.
         * @tparam T Atomic Type.
         * @param defaultValue Default value for atomic type T.
         * @returns T Node converted to specified atomic type.
         */
        template<typename T>
        T as(const T& defaultValue) const
        {
            return impl::StringConverter<T>::get(asString(), defaultValue);
        }

        /**
         * @brief Get size of node. Nodes of type None or Scalar will return 0.
         * @returns size_t Size of node.
         */
        size_t size() const;

        // Sequence operators
        /**
         * @brief Inserts a node in the sequence at the given index.
         * @param index Index to insert node.
         * @returns Node Added node. 
         */
        Node& insert(const size_t index);
        /**
         * @brief Add new sequence index to back. Converts node to sequence type if needed.
         * @returns Node Added node. 
         */
        Node& push_front();
        /**
         * @brief Add new sequence index to front. Converts node to sequence type if needed.
         * @returns Node Added node. 
         */
        Node& push_back();
        /**
         * @brief Get sequence/map item. Converts node to sequence/map type if needed.
         * @param index Index to get node.
         * @returns Node Node at specified index.
         */
        Node& operator[] (const size_t index);
        /**
         * @brief Get sequence/map item. Converts node to sequence/map type if needed.
         * @param key Key name.
         * @returns Node Node specified by key.
         */
        Node& operator[] (const std::string& key);

        /**
         * @brief Erase item. No action if node is not a sequence or map.
         * @param index Index to erase node.
         */
        void erase(const size_t index);
        /**
         * @brief Erase item. No action if node is not a sequence or map.
         * @param key Key Name.
         */
        void erase(const std::string& key);

        /**
         * @brief Assignment operator.
         * @param node Node instance to copy from.
         */
        Node& operator= (const Node& node);
        /**
         * @brief Assignment operator.
         * @param value String value to initialize node with.
         */
        Node& operator= (const std::string& value);
        /**
         * @brief Assignment operator.
         * @param value String value to initialize node with.
         */
        Node& operator= (const char* value);

        /**
         * @brief Get start iterator.
         * @returns Iterator Start iterator for sequence.
         */
        Iterator begin();
        /**
         * @brief Get start constant iterator.
         * @returns ConstIterator Start iterator for sequence.
         */
        ConstIterator begin() const;

        /**
         * @brief Get end iterator.
         * @returns Iterator End iterator for sequence.
         */
        Iterator end();
        /**
         * @brief Get end constant iterator.
         * @returns ConstIterator End iterator for sequence.
         */
        ConstIterator end() const;

    private:
        const std::string& asString() const;
        void* m_pImp;
    };

    /**
     * @brief Populate given root node with deserialized data.
     * @ingroup yaml
     * @param root Root node to populate.
     * @param filename Path of input file.
     */
    bool Parse(Node& root, const char* filename);
    /**
     * @brief Populate given root node with deserialized data.
     * @ingroup yaml
     * @param root Root node to populate.
     * @param stream Input stream.
     */
    bool Parse(Node& root, std::iostream& stream);
    /**
     * @brief Populate given root node with deserialized data.
     * @ingroup yaml
     * @param root Root node to populate.
     * @param string String of input data.
     */
    bool Parse(Node& root, const std::string& string);
    /**
     * @brief Populate given root node with deserialized data.
     * @ingroup yaml
     * @param buffer Character array of input data.
     * @param size Buffer size.
     */
    bool Parse(Node& root, const char* buffer, const size_t size);

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Serialization configuration structure, describing output behavior.
     * @ingroup yaml
     */
    struct SerializeConfig {
        /**
         * @brief Initializes a new instance of the SerializeConfig struct.
         * @param spaceIndentation Number of spaces per indentation.
         * @param scalarMaxLength Maximum length of scalars. Serialized as folder scalars if exceeded. Ignored if equal to 0.
         * @param sequenceMapNewline Put maps on a new line if parent node is a sequence.
         * @param mapScalarNewline Put scalars on a new line if parent node is a map.
         */
        SerializeConfig(const size_t spaceIndentation = 2, const size_t scalarMaxLength = 64, const bool sequenceMapNewline = false,
                        const bool mapScalarNewline = false);

        size_t SpaceIndentation;    // Number of spaces per indentation.
        size_t ScalarMaxLength;     // Maximum length of scalars. Serialized as folder scalars if exceeded.
        bool SequenceMapNewline;    // Put maps on a new line if parent node is a sequence.
        bool MapScalarNewline;      // Put scalars on a new line if parent node is a map.
    };

    /**
     * @brief Serialize node data.
     * @ingroup yaml
     * @param root Root node to serialize.
     * @param filename Path of output file.
     * @param config Serialization configuration.
     */
    void Serialize(const Node& root, const char* filename, const SerializeConfig& config = {2, 64, false, false});
    /**
     * @brief Serialize node data.
     * @ingroup yaml
     * @param root Root node to serialize.
     * @param stream Output stream.
     * @param config Serialization configuration.
     */
    void Serialize(const Node& root, std::iostream& stream, const SerializeConfig& config = {2, 64, false, false});
    /**
     * @brief Serialize node data.
     * @ingroup yaml
     * @param root Root node to serialize.
     * @param string String of output data.
     * @param config Serialization configuration.
     */
    void Serialize(const Node& root, std::string& string, const SerializeConfig& config = {2, 64, false, false});
} // namespace yaml

#endif // __YAML_H__
