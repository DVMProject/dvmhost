/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the mini-yaml project. (https://github.com/jimmiebergmann/mini-yaml)
// Licensed under the MIT License (https://opensource.org/licenses/MIT)
//
/*
*   Copyright(c) 2018 Jimmie Bergmann
*   Copyright (C) 2020 Bryan Biedenkapp N2PLL
*
*   MIT License
*
*   Permission is hereby granted, free of charge, to any person obtaining a copy
*   of this software and associated documentation files(the "Software"), to deal
*   in the Software without restriction, including without limitation the rights
*   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*   copies of the Software, and to permit persons to whom the Software is
*   furnished to do so, subject to the following conditions :
*
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*
*/
/*
* YAML documentation:
* http://yaml.org/spec/1.0/index.html
* https://www.codeproject.com/Articles/28720/YAML-Parser-in-C
*/
#if !defined(__YAML_H__)
#define __YAML_H__

#include "Defines.h"

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
        //      Helper functionality, converting string to any data type.
        //      Strings are left untouched.
        // ---------------------------------------------------------------------------

        template<typename T>
        struct StringConverter {
            /// <summary></summary>
            /// <param name="data"></param>
            /// <returns></returns>
            static T get(const std::string& data)
            {
                T type;
                std::stringstream ss(data);
                ss >> type;
                return type;
            }

            /// <summary></summary>
            /// <param name="data"></param>
            /// <param name="defaultValue"></param>
            /// <returns></returns>
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
        //
        // ---------------------------------------------------------------------------

        template<>
        struct StringConverter<std::string> {
            /// <summary></summary>
            /// <param name="data"></param>
            /// <returns></returns>
            static std::string get(const std::string& data)
            {
                return data;
            }

            /// <summary></summary>
            /// <param name="data"></param>
            /// <param name="defaultValue"></param>
            /// <returns></returns>
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
        //
        // ---------------------------------------------------------------------------

        template<>
        struct StringConverter<bool> {
            /// <summary></summary>
            /// <param name="data"></param>
            /// <returns></returns>
            static bool get(const std::string& data)
            {
                std::string tmpData = data;
                std::transform(tmpData.begin(), tmpData.end(), tmpData.begin(), ::tolower);
                if (tmpData == "true" || tmpData == "yes" || tmpData == "1") {
                    return true;
                }

                return false;
            }

            /// <summary></summary>
            /// <param name="data"></param>
            /// <param name="defaultValue"></param>
            /// <returns></returns>
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
    //      Exception class.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Exception : public std::runtime_error {
    public:

        /// <summary>
        /// Enumeration of exception types.
        /// </summary>
        enum eType
        {
            InternalError,  // Internal error.
            ParsingError,   // Invalid parsing data.
            OperationError  // User operation error.
        };

        /// <summary>Initializes a new instance of the Exception class.</summary>
        Exception(const std::string & message, const eType type);

        /// <summary>Get type of exception.</summary>
        eType type() const;

        /// <summary>Get message of exception.</summary>
        const char* message() const;

    private:
        eType m_Type;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Internal exception class.
    // ---------------------------------------------------------------------------
    
    class HOST_SW_API InternalException : public Exception {
    public:
        /// <summary>Initializes a new instance of the InternalException class.</summary>
        InternalException(const std::string& message);

    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Parsing exception class.
    // ---------------------------------------------------------------------------

    class HOST_SW_API ParsingException : public Exception {
    public:
        /// <summary>Initializes a new instance of the ParsingException class.</summary>
        ParsingException(const std::string & message);

    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Parsing exception class.
    // ---------------------------------------------------------------------------

    class HOST_SW_API OperationException : public Exception {
    public:
        /// <summary>Initializes a new instance of the OperationException class.</summary>
        OperationException(const std::string & message);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Iterator class.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Iterator {
    public:
        friend class Node;

        /// <summary>Initializes a new instance of the Iterator class.</summary>
        Iterator();
        /// <summary>Copies an instance of the Iterator class to a new instance of the Iterator class.</summary>
        Iterator(const Iterator & it);
        /// <summary>Finalizes a instance of the Iterator class.</summary>
        ~Iterator();

        /// <summary>Assignment operator.</summary>
        Iterator& operator = (const Iterator& it);

        /// <summary>Get node of iterator. First pair item is the key of map value, empty if type is sequence.</summary>
        std::pair<const std::string&, Node&> operator *();

        /// <summary>Post-increment operator.</summary>
        Iterator& operator ++ (int);
        /// <summary>Post-decrement operator.</summary>
        Iterator& operator -- (int);

        /// <summary>Check if iterator is equal to other iterator.</summary>
        bool operator == (const Iterator& it);

        /// <summary>Check if iterator is not equal to other iterator.</summary>
        bool operator != (const Iterator& it);

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
    //      Constant iterator class.
    // ---------------------------------------------------------------------------

    class HOST_SW_API ConstIterator {
    public:
        friend class Node;

        /// <summary>Initializes a new instance of the ConstIterator class.</summary>
        ConstIterator();
        /// <summary>Copies an instance of the ConstIterator class to a new instance of the ConstIterator class.</summary>
        ConstIterator(const ConstIterator & it);
        /// <summary>Finalizes a instance of the ConstIterator class.</summary>
        ~ConstIterator();

        /// <summary>Assignment operator.</summary>
        ConstIterator& operator = (const ConstIterator& it);

        /// <summary>Get node of iterator. First pair item is the key of map value, empty if type is sequence.</summary>
        std::pair<const std::string&, const Node&> operator *();

        /// <summary>Post-increment operator.</summary>
        ConstIterator& operator ++ (int);
        /// <summary>Post-decrement operator.</summary>
        ConstIterator& operator -- (int);

        /// <summary>Check if iterator is equal to other iterator.</summary>
        bool operator == (const ConstIterator& it);

        /// <summary>Check if iterator is not equal to other iterator.</summary>
        bool operator != (const ConstIterator& it);

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
    //      Node class.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Node {
    public:
        friend class Iterator;

        /// <summary>Enumeration of node types.</summary>
        enum eType
        {
            None,
            SequenceType,
            MapType,
            ScalarType
        };

        /// <summary>Initializes a new instance of the Node class.</summary>
        Node();
        /// <summary>Copies an instance of the Node class to a new instance of the Node class.</summary>
        Node(const Node& node);
        /// <summary>Initializes a new instance of the Node class.</summary>
        Node(const std::string& value);
        /// <summary>Initializes a new instance of the Node class.</summary>
        Node(const char* value);
        /// <summary>Finalizes a instance of the Node class.</summary>
        ~Node();

        /// <summary>Gets the type of node.</summary>
        eType type() const;
        /// <summary>Checks if the node contains nothing.</summary>
        bool isNone() const;
        /// <summary>Checks if the node is a sequence node.</summary>
        bool isSequence() const;
        /// <summary>Checks if the node is a map node.</summary>
        bool isMap() const;
        /// <summary>Checks if the node is a scalar node.</summary>
        bool isScalar() const;

        /// <summary>Completely clear node.</summary>
        void clear();

        /// <summary>Get node as given template type.</summary>
        template<typename T>
        T as() const
        {
            return impl::StringConverter<T>::get(asString());
        }
        /// <summary>Get node as given template type with a default value if no value is found.</summary>
        template<typename T>
        T as(const T& defaultValue) const
        {
            return impl::StringConverter<T>::get(asString(), defaultValue);
        }

        /// <summary>Get size of node. Nodes of type None or Scalar will return 0.</summary>
        size_t size() const;

        // Sequence operators
        /// <summary>Insert sequence item at given index. Converts node to sequence type if needed. 
        /// Adding new item to end of sequence if index is larger than sequence size.</summary>
        Node& insert(const size_t index);
        /// <summary>Add new sequence index to back. Converts node to sequence type if needed.</summary>
        Node& push_front();
        /// <summary>Add new sequence index to front. Converts node to sequence type if needed.</summary>
        Node& push_back();
        /// <summary>Get sequence/map item. Converts node to sequence/map type if needed.</summary>
        Node& operator [] (const size_t index);
        /// <summary>Get sequence/map item. Converts node to sequence/map type if needed.</summary>
        Node& operator [] (const std::string& key);

        /// <summary>Erase item. No action if node is not a sequence or map.</summary>
        void erase(const size_t index);
        /// <summary>Erase item. No action if node is not a sequence or map.</summary>
        void erase(const std::string& key);

        /// <summary>Assignment operator.</summary>
        Node& operator = (const Node& node);
        /// <summary>Assignment operator.</summary>
        Node& operator = (const std::string& value);
        /// <summary>Assignment operator.</summary>
        Node& operator = (const char* value);

        /// <summary>Get start iterator.</summary>
        Iterator begin();
        /// <summary>Get start constant iterator.</summary>
        ConstIterator begin() const;

        /// <summary>Get end iterator.</summary>
        Iterator end();
        /// <summary>Get end constant iterator.</summary>
        ConstIterator end() const;

    private:
        const std::string& asString() const;
        void* m_pImp;
    };

    /// <summary>Populate given root node with deserialized data.</summary>
    /// <param name="root">Root node to populate.</param>
    /// <param name="filename">Path of input file.</param>
    bool Parse(Node& root, const char* filename);
    /// <summary>Populate given root node with deserialized data.</summary>
    /// <param name="root">Root node to populate.</param>
    /// <param name="stream">Input stream.</param>
    bool Parse(Node& root, std::iostream& stream);
    /// <summary>Populate given root node with deserialized data.</summary>
    /// <param name="root">Root node to populate.</param>
    /// <param name="string">String of input data.</param>
    bool Parse(Node& root, const std::string& string);
    /// <summary>Populate given root node with deserialized data.</summary>
    /// <param name="buffer">Character array of input data.</param>
    /// <param name="size">Buffer size.</param>
    bool Parse(Node& root, const char* buffer, const size_t size);

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    //      Serialization configuration structure, describing output behavior.
    // ---------------------------------------------------------------------------

    struct SerializeConfig {
        /// <summary>Initializes a new instance of the SerializeConfig struct.</summary>
        /// <param name="spaceIndentation">Number of spaces per indentation.</param>
        /// <param name="scalarMaxLength">Maximum length of scalars. Serialized as folder scalars if exceeded. Ignored if equal to 0.</param>
        /// <param name="sequenceMapNewline">Put maps on a new line if parent node is a sequence.</param>
        /// <param name="mapScalarNewline">Put scalars on a new line if parent node is a map.</param>
        SerializeConfig(const size_t spaceIndentation = 2, const size_t scalarMaxLength = 64, const bool sequenceMapNewline = false,
                        const bool mapScalarNewline = false);

        size_t SpaceIndentation;    // Number of spaces per indentation.
        size_t ScalarMaxLength;     // Maximum length of scalars. Serialized as folder scalars if exceeded.
        bool SequenceMapNewline;    // Put maps on a new line if parent node is a sequence.
        bool MapScalarNewline;      // Put scalars on a new line if parent node is a map.
    };

    /// <summary>Serialize node data.</summary>
    /// <param name="root">Root node to serialize.</param>
    /// <param name="filename">Path of output file.</param>
    /// <param name="config">Serialization configuration.</param>
    void Serialize(const Node& root, const char* filename, const SerializeConfig& config = {2, 64, false, false});
    /// <summary>Serialize node data.</summary>
    /// <param name="root">Root node to serialize.</param>
    /// <param name="stream">Output stream.</param>
    /// <param name="config">Serialization configuration.</param>
    void Serialize(const Node& root, std::iostream& stream, const SerializeConfig& config = {2, 64, false, false});
    /// <summary>Serialize node data.</summary>
    /// <param name="root">Root node to serialize.</param>
    /// <param name="string">String of output data.</param>
    /// <param name="config">Serialization configuration.</param>
    void Serialize(const Node& root, std::string& string, const SerializeConfig& config = {2, 64, false, false});
} // namespace yaml

#endif // __YAML_H__
