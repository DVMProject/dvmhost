// SPDX-License-Identifier: MIT
/*
 * Digital Voice Modem - Common Library
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018 Jimmie Bergmann
 *  Copyright (C) 2020,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "yaml/Yaml.h"

#include <memory>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>

// Implementation access definitions.
#define NODE_IMP static_cast<NodeImp*>(m_pImp)
#define NODE_IMP_EXT(node) static_cast<NodeImp*>(node.m_pImp)
#define TYPE_IMP static_cast<NodeImp*>(m_pImp)->m_pImp

#define IT_IMP static_cast<IteratorImp*>(m_pImp)

namespace yaml
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class ReaderLine;

    // ---------------------------------------------------------------------------
    //  Namespace Globals
    // ---------------------------------------------------------------------------

    static yaml::Node g_NoneNode;

    // Exception message definitions.

    static const std::string g_ErrorInvalidCharacter        = "Invalid character found.";
    static const std::string g_ErrorKeyMissing              = "Missing key.";
    static const std::string g_ErrorKeyIncorrect            = "Incorrect key.";
    static const std::string g_ErrorValueIncorrect          = "Incorrect value.";
    static const std::string g_ErrorTabInOffset             = "Tab found in offset.";
    static const std::string g_ErrorBlockSequenceNotAllowed = "Sequence entries are not allowed in this context.";
    static const std::string g_ErrorUnexpectedDocumentEnd   = "Unexpected document end.";
    static const std::string g_ErrorDiffEntryNotAllowed     = "Different entry is not allowed in this context.";
    static const std::string g_ErrorIncorrectOffset         = "Incorrect offset.";
    static const std::string g_ErrorSequenceError           = "Error in sequence node.";
    static const std::string g_ErrorCannotOpenFile          = "Cannot open file.";
    static const std::string g_ErrorIndentation             = "Space indentation is less than 2.";
    static const std::string g_ErrorInvalidBlockScalar      = "Invalid block scalar.";
    static const std::string g_ErrorInvalidQuote            = "Invalid quote.";
    static const std::string g_EmptyString                  = "";

    // Global function definitions. Implemented at end of this source file.

    static std::string ExceptionMessage(const std::string& message, ReaderLine& line);
    static std::string ExceptionMessage(const std::string& message, ReaderLine& line, const size_t errorPos);
    static std::string ExceptionMessage(const std::string& message, const size_t errorLine, const size_t errorPos);
    static std::string ExceptionMessage(const std::string& message, const size_t errorLine, const std::string& data);

    static bool FindQuote(const std::string& input, size_t& start, size_t& end, size_t searchPos = 0);
    static size_t FindNotCited(const std::string& input, char token, size_t& preQuoteCount);
    static size_t FindNotCited(const std::string& input, char token);
    static bool ValidateQuote(const std::string& input);
    static void CopyNode(const Node& from, Node& to);
    static bool ShouldBeCited(const std::string& key);
    static void AddEscapeTokens(std::string& input, const std::string& tokens);
    static void RemoveAllEscapeTokens(std::string& input);

    // ---------------------------------------------------------------------------
    //  Public Class Members
    // ---------------------------------------------------------------------------

    /* Initializes a new instance of the Exception class. */
    Exception::Exception(const std::string& message, const eType type) :
        std::runtime_error(message),
        m_Type(type)
    {
        /* stub */
    }

    /* Get type of exception. */
    Exception::eType Exception::type() const
    {
        return m_Type;
    }

    /* Get message of exception. */
    const char* Exception::message() const
    {
        return what();
    }

    // ---------------------------------------------------------------------------
    //  Public Class Members
    // ---------------------------------------------------------------------------

    /* Initializes a new instance of the InternalException class. */
    InternalException::InternalException(const std::string& message) :
        Exception(message, InternalError)
    {
        /* stub */
    }

    // ---------------------------------------------------------------------------
    //  Public Class Members
    // ---------------------------------------------------------------------------

    /* Initializes a new instance of the ParsingException class. */
    ParsingException::ParsingException(const std::string& message) :
        Exception(message, ParsingError)
    {
        /* stub */
    }

    // ---------------------------------------------------------------------------
    //  Public Class Members
    // ---------------------------------------------------------------------------

    /* Initializes a new instance of the OperationException class. */
    OperationException::OperationException(const std::string & message) :
        Exception(message, OperationError)
    {
        /* stub */
    }

    /*
    ** Type implementations
    */

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief TypeImp YAML type implementation.
     */
    class TypeImp {
    public:
        /* Finalizes a new instance of the TypeImp class. */
        virtual ~TypeImp() = default;

        /*  */
        virtual const std::string& getData() const = 0;
        /*  */
        virtual bool setData(const std::string& data) = 0;

        /*  */
        virtual size_t size() const = 0;

        /*  */
        virtual Node* getNode(const size_t index) = 0;
        /*  */
        virtual Node* getNode(const std::string& key) = 0;
        /*  */
        virtual Node* insert(const size_t index) = 0;
        /*  */
        virtual Node* push_front() = 0;
        /*  */
        virtual Node* push_back() = 0;
        /*  */
        virtual void erase(const size_t index) = 0;
        /*  */
        virtual void erase(const std::string& key) = 0;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief SequenceImp YAML sequence implementation.
     */
    class SequenceImp : public TypeImp {
    public:
        /* Finalizes a new instance of the SequenceImp class. */
        ~SequenceImp() override
        {
            for (auto it = m_Sequence.begin(); it != m_Sequence.end(); it++) {
                delete it->second;
            }
        }

        /*  */
        virtual const std::string& getData() const { return g_EmptyString; }
        /*  */
        virtual bool setData(const std::string & data) { return false; }

        /*  */
        virtual size_t size() const { return m_Sequence.size(); }

        /*  */
        virtual Node* getNode(const size_t index)
        {
            auto it = m_Sequence.find(index);
            if (it != m_Sequence.end()) {
                return it->second;
            }
            return nullptr;
        }
        /*  */
        virtual Node* getNode(const std::string& key) { return nullptr; }

        /*  */
        virtual Node* insert(const size_t index)
        {
            if (m_Sequence.size() == 0) {
                Node* pNode = new Node;
                m_Sequence.insert({ 0, pNode });
                return pNode;
            }

            if (index >= m_Sequence.size()) {
                auto it = m_Sequence.end();
                --it;
                Node* pNode = new Node;
                m_Sequence.insert({ it->first, pNode });
                return pNode;
            }

            auto it = m_Sequence.cbegin();
            while (it != m_Sequence.cend()) {
                m_Sequence[it->first + 1] = it->second;
                if (it->first == index) {
                    break;
                }
            }

            Node* pNode = new Node;
            m_Sequence.insert({ index, pNode });
            return pNode;
        }
        /*  */
        virtual Node* push_front()
        {
            for (auto it = m_Sequence.cbegin(); it != m_Sequence.cend(); it++) {
                m_Sequence[it->first + 1] = it->second;
            }

            Node* pNode = new Node;
            m_Sequence.insert({ 0, pNode });
            return pNode;
        }
        /*  */
        virtual Node* push_back()
        {
            size_t index = 0;
            if (m_Sequence.size()) {
                auto it = m_Sequence.end();
                --it;
                index = it->first + 1;
            }

            Node* pNode = new Node;
            m_Sequence.insert({ index, pNode });
            return pNode;
        }

        /*  */
        virtual void erase(const size_t index)
        {
            auto it = m_Sequence.find(index);
            if (it == m_Sequence.end()) {
                return;
            }
            delete it->second;
            m_Sequence.erase(index);
        }
        /*  */
        virtual void erase(const std::string& key) { /* stub */ }

    public:
        std::map<size_t, Node*> m_Sequence;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief MapImp YAML map implementation.
     */
    class MapImp : public TypeImp {
    public:
        /* Finalizes a new instance of the MapImp class. */
        ~MapImp() override
        {
            for (auto it = m_Map.begin(); it != m_Map.end(); it++) {
                delete it->second;
            }
        }

        /*  */
        virtual const std::string& getData() const { return g_EmptyString; }
        /*  */
        virtual bool setData(const std::string& data) { return false; }

        /*  */
        virtual size_t size() const { return m_Map.size(); }

        /*  */
        virtual Node* getNode(const size_t index) { return nullptr; }
        /*  */
        virtual Node* getNode(const std::string& key)
        {
            auto it = m_Map.find(key);
            if (it == m_Map.end()) {
                Node* pNode = new Node;
                m_Map.insert({ key, pNode });
                return pNode;
            }
            return it->second;
        }

        /*  */
        virtual Node* insert(const size_t index) { return nullptr; }
        /*  */
        virtual Node* push_front() { return nullptr; }
        /*  */
        virtual Node* push_back() { return nullptr; }
        
        /*  */
        virtual void erase(const size_t index) { /* stub */ }
        /*  */
        virtual void erase(const std::string& key)
        {
            auto it = m_Map.find(key);
            if (it == m_Map.end()) {
                return;
            }
            delete it->second;
            m_Map.erase(key);
        }

    public:
        std::map<std::string, Node*> m_Map;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief MapImp YAML scalar implementation.
     */
    class ScalarImp : public TypeImp {
    public:
        /* Finalizes a new instance of the ScalarImp class. */
        ~ScalarImp() override { /* stub */ }

        /*  */
        virtual const std::string& getData() const { return m_Value; }
        /*  */
        virtual bool setData(const std::string& data)
        {
            m_Value = data;
            return true;
        }

        /*  */
        virtual size_t size() const { return 0; }

        /*  */
        virtual Node* getNode(const size_t index) { return nullptr; }
        /*  */
        virtual Node* getNode(const std::string& key) { return nullptr; }
    
        /*  */
        virtual Node* insert(const size_t index) { return nullptr; }
        /*  */
        virtual Node* push_front() { return nullptr; }
        /*  */
        virtual Node* push_back() { return nullptr; }
        /*  */
        virtual void erase(const size_t index) { /* stub */ }
        /*  */
        virtual void erase(const std::string& key) { /* stub */ }

    public:
        std::string m_Value;
    };

    /*
    ** Node implementation
    */

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief NodeImp YAML node implementation.
     */
    class NodeImp {
    public:
        /* Initializes a new instance of the NodeImp class. */
        NodeImp() :
            m_Type(Node::None),
            m_pImp(nullptr)
        {
            /* stub */
        }
        /* Finalizes a new instance of the NodeImp class. */
        ~NodeImp() { clear(); }

        /* Completely clear node. */
        void clear()
        {
            if (m_pImp != nullptr) {
                delete m_pImp;
                m_pImp = nullptr;
            }
            m_Type = Node::None;
        }

        /*  */
        void initSequence()
        {
            if (m_Type != Node::SequenceType || m_pImp == nullptr) {
                if (m_pImp) {
                    delete m_pImp;
                }
                m_pImp = new SequenceImp;
                m_Type = Node::SequenceType;
            }
        }
        /*  */
        void initMap()
        {
            if (m_Type != Node::MapType || m_pImp == nullptr) {
                if (m_pImp) {
                    delete m_pImp;
                }
                m_pImp = new MapImp;
                m_Type = Node::MapType;
            }
        }
        /*  */
        void initScalar()
        {
            if (m_Type != Node::ScalarType || m_pImp == nullptr) {
                if (m_pImp) {
                    delete m_pImp;
                }
                m_pImp = new ScalarImp;
                m_Type = Node::ScalarType;
            }

        }

    public:
        Node::eType m_Type;
        TypeImp* m_pImp;
    };

    /*
    ** Iterator implementations
    */

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief IteratorImp YAML iterator implementation.
     */
    class IteratorImp {
    public:
        /* Finalizes a new instance of the IteratorImp class. */
        virtual ~IteratorImp() = default;

        /*  */
        virtual Node::eType type() const = 0;
        /*  */
        virtual void initBegin(SequenceImp* pSequenceImp) = 0;
        /*  */
        virtual void initEnd(SequenceImp* pSequenceImp) = 0;
        /*  */
        virtual void initBegin(MapImp* pMapImp) = 0;
        /*  */
        virtual void initEnd(MapImp* pMapImp) = 0;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief SequenceIteratorImp YAML sequence iterator implementation.
     */
    class SequenceIteratorImp : public IteratorImp {
    public:
        /*  */
        Node::eType type() const override { return Node::SequenceType; }

        /*  */
        void initBegin(SequenceImp* pSequenceImp) override { m_Iterator = pSequenceImp->m_Sequence.begin(); }
        /*  */
        void initEnd(SequenceImp* pSequenceImp) override { m_Iterator = pSequenceImp->m_Sequence.end(); }
        /*  */
        void initBegin(MapImp* pMapImp) override { /* stub */ }
        /*  */
        void initEnd(MapImp* pMapImp) override { /* stub */ }

        /*  */
        void copy(const SequenceIteratorImp& it) { m_Iterator = it.m_Iterator; }

    public:
        std::map<size_t, Node*>::iterator m_Iterator;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief MapIteratorImp YAML map iterator implementation.
     */
    class MapIteratorImp : public IteratorImp {
    public:
        /*  */
        Node::eType type() const override { return Node::MapType; }

        /*  */
        void initBegin(SequenceImp* pSequenceImp) override { /* stub */ }
        /*  */
        void initEnd(SequenceImp* pSequenceImp) override { /* stub */ }
        /*  */
        void initBegin(MapImp* pMapImp) override { m_Iterator = pMapImp->m_Map.begin(); }
        /*  */
        void initEnd(MapImp* pMapImp) override { m_Iterator = pMapImp->m_Map.end(); }

        /*  */
        void copy(const MapIteratorImp& it) { m_Iterator = it.m_Iterator; }

    public:
        std::map<std::string, Node*>::iterator m_Iterator;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief SequenceConstIteratorImp YAML constant sequence iterator implementation.
     */
    class SequenceConstIteratorImp : public IteratorImp {
    public:
        /*  */
        Node::eType type() const override { return Node::SequenceType; }

        /*  */
        void initBegin(SequenceImp* pSequenceImp) override { m_Iterator = pSequenceImp->m_Sequence.begin(); }
        /*  */
        void initEnd(SequenceImp* pSequenceImp) override { m_Iterator = pSequenceImp->m_Sequence.end(); }
        /*  */
        void initBegin(MapImp* pMapImp) override { /* stub */ }
        /*  */
        void initEnd(MapImp* pMapImp) override { /* stub */ }

        /*  */
        void copy(const SequenceConstIteratorImp& it) { m_Iterator = it.m_Iterator; }

    public:
        std::map<size_t, Node*>::const_iterator m_Iterator;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief MapConstIteratorImp YAML constant map iterator implementation.
     */
    class MapConstIteratorImp : public IteratorImp {
    public:
        /*  */
        Node::eType type() const override { return Node::MapType; }

        /*  */
        void initBegin(SequenceImp* pSequenceImp) override { /* stub */ }
        /*  */
        void initEnd(SequenceImp* pSequenceImp) override { /* stub */ }
        /*  */
        void initBegin(MapImp* pMapImp) override { m_Iterator = pMapImp->m_Map.begin(); }
        /*  */
        void initEnd(MapImp* pMapImp) override { m_Iterator = pMapImp->m_Map.end(); }

        /*  */
        void copy(const MapConstIteratorImp& it) { m_Iterator = it.m_Iterator; }

    public:
        std::map<std::string, Node*>::const_iterator m_Iterator;
    };

    // ---------------------------------------------------------------------------
    //  Public Class Members
    // ---------------------------------------------------------------------------

    /* Initializes a new instance of the Iterator class. */
    Iterator::Iterator() :
        m_Type(None),
        m_pImp(nullptr)
    {
        /* stub */
    }
    /* Copies an instance of the Iterator class to a new instance of the Iterator class. */
    Iterator::Iterator(const Iterator& it) :
        m_Type(None),
        m_pImp(nullptr)
    {
        *this = it;
    }
    /* Finalizes a instance of the Iterator class. */
    Iterator::~Iterator()
    {
        if (m_pImp) {
            switch (m_Type) {
            case SequenceType:
                delete static_cast<SequenceIteratorImp*>(m_pImp);
                break;
            case MapType:
                delete static_cast<MapIteratorImp*>(m_pImp);
                break;
            default:
                break;
            }

        }
    }

    /* Assignment operator. */
    Iterator& Iterator::operator= (const Iterator& it)
    {
        if (m_pImp) {
            switch (m_Type) {
            case SequenceType:
                delete static_cast<SequenceIteratorImp*>(m_pImp);
                break;
            case MapType:
                delete static_cast<MapIteratorImp*>(m_pImp);
                break;
            default:
                break;
            }

            m_pImp = nullptr;
            m_Type = None;
        }

        IteratorImp* pNewImp = nullptr;
        switch (it.m_Type) {
        case SequenceType:
            m_Type = SequenceType;
            pNewImp = new SequenceIteratorImp;
            static_cast<SequenceIteratorImp*>(pNewImp)->m_Iterator = static_cast<SequenceIteratorImp*>(it.m_pImp)->m_Iterator;
            break;
        case MapType:
            m_Type = MapType;
            pNewImp = new MapIteratorImp;
            static_cast<MapIteratorImp*>(pNewImp)->m_Iterator = static_cast<MapIteratorImp*>(it.m_pImp)->m_Iterator;
            break;
        default:
            break;
        }

        m_pImp = pNewImp;
        return *this;
    }

    /* Get node of iterator. First pair item is the key of map value, empty if type is sequence. */
    std::pair<const std::string&, Node&> Iterator::operator* ()
    {
        switch (m_Type) {
        case SequenceType:
            return { g_EmptyString, *(static_cast<SequenceIteratorImp*>(m_pImp)->m_Iterator->second) };
            break;
        case MapType:
            return { static_cast<MapIteratorImp*>(m_pImp)->m_Iterator->first,
                    *(static_cast<MapIteratorImp*>(m_pImp)->m_Iterator->second) };
            break;
        default:
            break;
        }

        g_NoneNode.clear();
        return { g_EmptyString, g_NoneNode };
    }

    /* Post-increment operator. */
    Iterator& Iterator::operator++ (int dummy)
    {
        switch (m_Type) {
        case SequenceType:
            static_cast<SequenceIteratorImp*>(m_pImp)->m_Iterator++;
            break;
        case MapType:
            static_cast<MapIteratorImp*>(m_pImp)->m_Iterator++;
            break;
        default:
            break;
        }
        return *this;
    }

    /* Post-decrement operator. */
    Iterator& Iterator::operator-- (int dummy)
    {
        switch(m_Type) {
        case SequenceType:
            static_cast<SequenceIteratorImp*>(m_pImp)->m_Iterator--;
            break;
        case MapType:
            static_cast<MapIteratorImp*>(m_pImp)->m_Iterator--;
            break;
        default:
            break;
        }
        return *this;
    }

    /* Check if iterator is equal to other iterator. */
    bool Iterator::operator== (const Iterator& it)
    {
        if (m_Type != it.m_Type) {
            return false;
        }

        switch (m_Type) {
        case SequenceType:
            return static_cast<SequenceIteratorImp*>(m_pImp)->m_Iterator == static_cast<SequenceIteratorImp*>(it.m_pImp)->m_Iterator;
            break;
        case MapType:
            return static_cast<MapIteratorImp*>(m_pImp)->m_Iterator == static_cast<MapIteratorImp*>(it.m_pImp)->m_Iterator;
            break;
        default:
            break;
        }

        return false;
    }

    /* Check if iterator is not equal to other iterator. */
    bool Iterator::operator!= (const Iterator& it)
    {
        return !(*this == it);
    }

    // ---------------------------------------------------------------------------
    //  Public Class Members
    // ---------------------------------------------------------------------------

    /* Initializes a new instance of the ConstIterator class. */
    ConstIterator::ConstIterator() :
        m_Type(None),
        m_pImp(nullptr)
    {
        /* stub */
    }
    /* Copies an instance of the ConstIterator class to a new instance of the ConstIterator class. */
    ConstIterator::ConstIterator(const ConstIterator& it) :
        m_Type(None),
        m_pImp(nullptr)
    {
        *this = it;
    }
    /* Finalizes a instance of the ConstIterator class. */
    ConstIterator::~ConstIterator()
    {
        if (m_pImp) {
            switch (m_Type) {
            case SequenceType:
                delete static_cast<SequenceConstIteratorImp*>(m_pImp);
                break;
            case MapType:
                delete static_cast<MapConstIteratorImp*>(m_pImp);
                break;
            default:
                break;
            }

        }
    }

    /* Assignment operator. */
    ConstIterator& ConstIterator::operator= (const ConstIterator& it)
    {
        if (m_pImp) {
            switch (m_Type) {
            case SequenceType:
                delete static_cast<SequenceConstIteratorImp*>(m_pImp);
                break;
            case MapType:
                delete static_cast<MapConstIteratorImp*>(m_pImp);
                break;
            default:
                break;
            }
            m_pImp = nullptr;
            m_Type = None;
        }

        IteratorImp* pNewImp = nullptr;
        switch (it.m_Type) {
        case SequenceType:
            m_Type = SequenceType;
            pNewImp = new SequenceConstIteratorImp;
            static_cast<SequenceConstIteratorImp*>(pNewImp)->m_Iterator = static_cast<SequenceConstIteratorImp*>(it.m_pImp)->m_Iterator;
            break;
        case MapType:
            m_Type = MapType;
            pNewImp = new MapConstIteratorImp;
            static_cast<MapConstIteratorImp*>(pNewImp)->m_Iterator = static_cast<MapConstIteratorImp*>(it.m_pImp)->m_Iterator;
            break;
        default:
            break;
        }

        m_pImp = pNewImp;
        return *this;
    }

    /* Get node of iterator. First pair item is the key of map value, empty if type is sequence. */
    std::pair<const std::string&, const Node&> ConstIterator::operator* ()
    {
        switch (m_Type) {
        case SequenceType:
            return { g_EmptyString, *(static_cast<SequenceConstIteratorImp*>(m_pImp)->m_Iterator->second) };
            break;
        case MapType:
            return { static_cast<MapConstIteratorImp*>(m_pImp)->m_Iterator->first,
                    *(static_cast<MapConstIteratorImp*>(m_pImp)->m_Iterator->second) };
            break;
        default:
            break;
        }

        g_NoneNode.clear();
        return { g_EmptyString, g_NoneNode };
    }

    /* Post-increment operator. */
    ConstIterator& ConstIterator::operator++ (int dummy)
    {
        switch (m_Type) {
        case SequenceType:
            static_cast<SequenceConstIteratorImp*>(m_pImp)->m_Iterator++;
            break;
        case MapType:
            static_cast<MapConstIteratorImp*>(m_pImp)->m_Iterator++;
            break;
        default:
            break;
        }
        return *this;
    }

    /* Post-decrement operator. */
    ConstIterator& ConstIterator::operator-- (int dummy)
    {
        switch (m_Type) {
        case SequenceType:
            static_cast<SequenceConstIteratorImp*>(m_pImp)->m_Iterator--;
            break;
        case MapType:
            static_cast<MapConstIteratorImp*>(m_pImp)->m_Iterator--;
            break;
        default:
            break;
        }
        return *this;
    }

    /* Check if iterator is equal to other iterator. */
    bool ConstIterator::operator== (const ConstIterator& it)
    {
        if (m_Type != it.m_Type) {
            return false;
        }

        switch (m_Type) {
        case SequenceType:
            return static_cast<SequenceConstIteratorImp*>(m_pImp)->m_Iterator == static_cast<SequenceConstIteratorImp*>(it.m_pImp)->m_Iterator;
            break;
        case MapType:
            return static_cast<MapConstIteratorImp*>(m_pImp)->m_Iterator == static_cast<MapConstIteratorImp*>(it.m_pImp)->m_Iterator;
            break;
        default:
            break;
        }

        return false;
    }

    /* Check if iterator is not equal to other iterator. */
    bool ConstIterator::operator!= (const ConstIterator & it)
    {
        return !(*this == it);
    }

    // ---------------------------------------------------------------------------
    //  Public Class Members
    // ---------------------------------------------------------------------------

    /* Initializes a new instance of the Node class. */
    Node::Node() :
        m_pImp(new NodeImp)
    {
        /* stub */
    }
    /* Copies an instance of the Node class to a new instance of the Node class. */
    Node::Node(const Node& node) :
        Node()
    {
        *this = node;
    }
    /* Initializes a new instance of the Node class. */
    Node::Node(const std::string& value) :
        Node()
    {
        *this = value;
    }
    /* Initializes a new instance of the Node class. */
    Node::Node(const char* value) :
        Node()
    {
        *this = value;
    }
    /* Finalizes a instance of the Node class. */
    Node::~Node()
    {
        delete static_cast<NodeImp*>(m_pImp);
    }

    /* Gets the type of node. */
    Node::eType Node::type() const
    {
        return NODE_IMP->m_Type;
    }

    /* Checks if the node contains nothing. */
    bool Node::isNone() const
    {
        return NODE_IMP->m_Type == Node::None;
    }

    /* Checks if the node is a sequence node. */
    bool Node::isSequence() const
    {
        return NODE_IMP->m_Type == Node::SequenceType;
    }

    /* Checks if the node is a map node. */
    bool Node::isMap() const
    {
        return NODE_IMP->m_Type == Node::MapType;
    }

    /* Checks if the node is a scalar node. */
    bool Node::isScalar() const
    {
        return NODE_IMP->m_Type == Node::ScalarType;
    }

    /* Completely clear node. */
    void Node::clear()
    {
        NODE_IMP->clear();
    }

    /* Get size of node. Nodes of type None or Scalar will return 0. */
    size_t Node::size() const
    {
        if (TYPE_IMP == nullptr) {
            return 0;
        }

        return TYPE_IMP->size();
    }

    /* Insert sequence item at given index. Converts node to sequence type if needed. Adding new item to end of sequence if index is larger than sequence size. */
    Node& Node::insert(const size_t index)
    {
        NODE_IMP->initSequence();
        return *TYPE_IMP->insert(index);
    }

    /* Add new sequence index to back. Converts node to sequence type if needed. */
    Node& Node::push_front()
    {
        NODE_IMP->initSequence();
        return *TYPE_IMP->push_front();
    }
    /* Add new sequence index to front. Converts node to sequence type if needed. */
    Node& Node::push_back()
    {
        NODE_IMP->initSequence();
        return *TYPE_IMP->push_back();
    }

    /* Get sequence/map item. Converts node to sequence/map type if needed. */
    Node& Node::operator[](const size_t index)
    {
        NODE_IMP->initSequence();
        Node* pNode = TYPE_IMP->getNode(index);
        if (pNode == nullptr) {
            g_NoneNode.clear();
            return g_NoneNode;
        }
        return *pNode;
    }
    /* Get sequence/map item. Converts node to sequence/map type if needed. */
    Node& Node::operator[](const std::string& key)
    {
        NODE_IMP->initMap();
        return *TYPE_IMP->getNode(key);
    }

    /* Erase item. No action if node is not a sequence or map. */
    void Node::erase(const size_t index)
    {
        if (TYPE_IMP == nullptr || NODE_IMP->m_Type != Node::SequenceType) {
            return;
        }

        return TYPE_IMP->erase(index);
    }
    /* Erase item. No action if node is not a sequence or map. */
    void Node::erase(const std::string& key)
    {
        if (TYPE_IMP == nullptr || NODE_IMP->m_Type != Node::MapType) {
            return;
        }

        return TYPE_IMP->erase(key);
    }

    /* Assignment operator. */
    Node& Node::operator= (const Node& node)
    {
        NODE_IMP->clear();
        CopyNode(node, *this);
        return *this;
    }
    /* Assignment operator. */
    Node& Node::operator= (const std::string& value)
    {
        NODE_IMP->initScalar();
        TYPE_IMP->setData(value);
        return *this;
    }
    /* Assignment operator. */
    Node& Node::operator= (const char* value)
    {
        NODE_IMP->initScalar();
        TYPE_IMP->setData(value ? std::string(value) : "");
        return *this;
    }

    /* Get start iterator. */
    Iterator Node::begin()
    {
        Iterator it;
        if (TYPE_IMP != nullptr) {
            IteratorImp* pItImp = nullptr;
            switch (NODE_IMP->m_Type) {
            case Node::SequenceType:
                it.m_Type = Iterator::SequenceType;
                pItImp = new SequenceIteratorImp;
                pItImp->initBegin(static_cast<SequenceImp*>(TYPE_IMP));
                break;
            case Node::MapType:
                it.m_Type = Iterator::MapType;
                pItImp = new MapIteratorImp;
                pItImp->initBegin(static_cast<MapImp*>(TYPE_IMP));
                break;
            default:
                break;
            }

            it.m_pImp = pItImp;
        }

        return it;
    }
    /* Get start constant iterator. */
    ConstIterator Node::begin() const
    {
        ConstIterator it;
        if (TYPE_IMP != nullptr) {
            IteratorImp* pItImp = nullptr;
            switch (NODE_IMP->m_Type) {
            case Node::SequenceType:
                it.m_Type = ConstIterator::SequenceType;
                pItImp = new SequenceConstIteratorImp;
                pItImp->initBegin(static_cast<SequenceImp*>(TYPE_IMP));
                break;
            case Node::MapType:
                it.m_Type = ConstIterator::MapType;
                pItImp = new MapConstIteratorImp;
                pItImp->initBegin(static_cast<MapImp*>(TYPE_IMP));
                break;
            default:
                break;
            }

            it.m_pImp = pItImp;
        }

        return it;
    }

    /* Get end iterator. */
    Iterator Node::end()
    {
       Iterator it;
        if (TYPE_IMP != nullptr) {
            IteratorImp* pItImp = nullptr;
            switch (NODE_IMP->m_Type) {
            case Node::SequenceType:
                it.m_Type = Iterator::SequenceType;
                pItImp = new SequenceIteratorImp;
                pItImp->initEnd(static_cast<SequenceImp*>(TYPE_IMP));
                break;
            case Node::MapType:
                it.m_Type = Iterator::MapType;
                pItImp = new MapIteratorImp;
                pItImp->initEnd(static_cast<MapImp*>(TYPE_IMP));
                break;
            default:
                break;
            }

            it.m_pImp = pItImp;
        }

        return it;
    }
    /* Get end constant iterator. */
    ConstIterator Node::end() const
    {
        ConstIterator it;
        if (TYPE_IMP != nullptr) {
            IteratorImp* pItImp = nullptr;
            switch (NODE_IMP->m_Type) {
            case Node::SequenceType:
                it.m_Type = ConstIterator::SequenceType;
                pItImp = new SequenceConstIteratorImp;
                pItImp->initEnd(static_cast<SequenceImp*>(TYPE_IMP));
                break;
            case Node::MapType:
                it.m_Type = ConstIterator::MapType;
                pItImp = new MapConstIteratorImp;
                pItImp->initEnd(static_cast<MapImp*>(TYPE_IMP));
                break;
            default:
                break;
            }

            it.m_pImp = pItImp;
        }

        return it;
    }

    // ---------------------------------------------------------------------------
    //  Private Class Members
    // ---------------------------------------------------------------------------

    /*  */
    const std::string& Node::asString() const
    {
        if (TYPE_IMP == nullptr) {
            return g_EmptyString;
        }

        return TYPE_IMP->getData();
    }

    /*
    ** Reader implementations
    */

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief ReaderLine Text line reader.
     */
    class ReaderLine {
    public:
        /* Initializes a new instance of the ReaderLine class. */
        ReaderLine(const std::string& data = "", const size_t no = 0, const size_t offset = 0,
                   const Node::eType type = Node::None, const uint8_t flags = 0) :
            Data(data),
            No(no),
            Offset(offset),
            Type(type),
            Flags(flags),
            NextLine(nullptr)
        {
            /* stub */
        }

        enum eFlag {
            LiteralScalarFlag,      // Literal scalar type, defined as "|".
            FoldedScalarFlag,       // Folded scalar type, defined as "<".
            ScalarNewlineFlag       // Scalar ends with a newline.
        };

        /* Set flag. */
        void setFlag(const eFlag flag) { Flags |= FlagMask[static_cast<size_t>(flag)]; }
        /* Set flags by mask value. */
        void setFlags(const uint8_t flags) { Flags |= flags; }

        /* Unset flag. */
        void unsetFlag(const eFlag flag) { Flags &= ~FlagMask[static_cast<size_t>(flag)]; }
        /* Unset flags by mask value. */
        void unsetFlags(const uint8_t flags) { Flags &= ~flags; }

        /* Get flag value. */
        bool getFlag(const eFlag flag) const { return Flags & FlagMask[static_cast<size_t>(flag)]; }

        /* Copy and replace scalar flags from another ReaderLine. */
        void copyScalarFlags(ReaderLine* from)
        {
            if (from == nullptr) {
                return;
            }

            unsigned char newFlags = from->Flags & (FlagMask[0] | FlagMask[1] | FlagMask[2]);
            Flags |= newFlags;
        }

    public:
        static const unsigned char FlagMask[3];

        std::string Data;       // Data of line.
        size_t No;              // Line number.
        size_t Offset;          // Offset to first character in data.
        Node::eType Type;       // Type of line.
        unsigned char Flags;    // Flags of line.
        ReaderLine* NextLine;   // Pointer to next line.
    };

    const unsigned char ReaderLine::FlagMask[3] = { 0x01, 0x02, 0x04 };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief ParseImp YAML parser implementation.
     * Parses an incoming stream and outputs a YAML root node.
     */
    class ParseImp {
    public:
        /* Initializes a new instance of the ParseImp class. */
        ParseImp() = default;
        /* Finalizes a new instance of the ParseImp class. */
        ~ParseImp() { clearLines(); }

        /* Run full parsing procedure. */
        void parse(Node& root, std::iostream& stream)
        {
            try
            {
                root.clear();
                readLines(stream);
                postProcessLines();
                parseRoot(root);
            }
            catch (Exception const& e)
            {
                root.clear();
                throw;
            }
        }

    private:
        /* Copies a instance of the ParseImp class to new instance of the ParseImp class. */
        ParseImp(const ParseImp& copy) { /* stub */ }

        /* Read all lines. */
        void readLines(std::iostream& stream)
        {
            std::string line = "";
            size_t lineNo = 0;
            bool documentStartFound = false;
            bool foundFirstNotEmpty = false;
            std::streampos streamPos = 0;

            // read all lines, as long as the stream is ok
            while (!stream.eof() && !stream.fail()) {
                // read line
                streamPos = stream.tellg();
                std::getline(stream, line);
                lineNo++;

                // remove comment
                const size_t commentPos = FindNotCited(line, '#');
                if (commentPos != std::string::npos) {
                    line.resize(commentPos);
                }

                // start of document
                if (!documentStartFound && line == "---") {
                    // erase all lines before this line
                    clearLines();
                    documentStartFound = true;
                    continue;
                }

                // end of document
                if (line == "...") {
                    break;
                }
                else if (line == "---") {
                    stream.seekg(streamPos);
                    break;
                }

                // remove trailing return
                if (line.size()) {
                    if (line[line.size() - 1] == '\r') {
                        line.resize(line.size() - 1);
                    }
                }

                // validate characters
                for (size_t i = 0; i < line.size(); i++) {
                    if (line[i] != '\t' && (line[i] < 32 || line[i] > 125)) {
                        throw ParsingException(ExceptionMessage(g_ErrorInvalidCharacter, lineNo, i + 1));
                    }
                }

                // validate tabs
                const size_t firstTabPos = line.find_first_of('\t');
                size_t startOffset = line.find_first_not_of(" \t");

                // make sure no tabs are in the very front
                if (startOffset != std::string::npos) {
                    if (firstTabPos < startOffset) {
                        throw ParsingException(ExceptionMessage(g_ErrorTabInOffset, lineNo, firstTabPos));
                    }

                    // remove front spaces
                    line = line.substr(startOffset);
                }
                else {
                    startOffset = 0;
                    line = "";
                }

                // add line
                if (!foundFirstNotEmpty) {
                    if (line.size()) {
                        foundFirstNotEmpty = true;
                    }
                    else {
                        continue;
                    }
                }

                ReaderLine* pLine = new ReaderLine(line, lineNo, startOffset);
                m_Lines.push_back(pLine);
            }
        }

        /* Run post-processing on all lines. Basically split lines into multiple lines if needed, to follow the parsing algorithm. */
        void postProcessLines()
        {
            for (auto it = m_Lines.begin(); it != m_Lines.end();) {
                // sequence
                if (postProcessSequenceLine(it)) {
                    continue;
                }

                // mapping
                if (postProcessMappingLine(it)) {
                    continue;
                }

                // scalar
                postProcessScalarLine(it);
            }

            // set next line of all lines
            if (m_Lines.size()) {
                if (m_Lines.back()->Type != Node::ScalarType) {
                    throw ParsingException(ExceptionMessage(g_ErrorUnexpectedDocumentEnd, *m_Lines.back()));
                }

                if (m_Lines.size() > 1) {
                    auto prevEnd = m_Lines.end();
                    --prevEnd;

                    for (auto it = m_Lines.begin(); it != prevEnd; it++) {
                        auto nextIt = it;
                        ++nextIt;

                        (*it)->NextLine = *nextIt;
                    }
                }
            }
        }

        /* Run post-processing and check for sequence. Split line into two lines if sequence token is not on it's own line. */
        bool postProcessSequenceLine(std::list<ReaderLine*>::iterator & it)
        {
            ReaderLine* pLine = *it;

            // sequence split
            if (!isSequenceStart(pLine->Data)) {
                return false;
            }

            pLine->Type = Node::SequenceType;
            clearTrailingEmptyLines(++it);

            const size_t valueStart = pLine->Data.find_first_not_of(" \t", 1);
            if (valueStart == std::string::npos) {
                return true;
            }

            // create new line and insert
            std::string newLine = pLine->Data.substr(valueStart);
            it = m_Lines.insert(it, new ReaderLine(newLine, pLine->No, pLine->Offset + valueStart));
            pLine->Data = "";

            return false;
        }

        /* Run post-processing and check for mapping. Split line into two lines if mapping value is not on it's own line. */
        bool postProcessMappingLine(std::list<ReaderLine*>::iterator & it)
        {
            ReaderLine* pLine = *it;

            // find map key
            size_t preKeyQuotes = 0;
            size_t tokenPos = FindNotCited(pLine->Data, ':', preKeyQuotes);
            if (tokenPos == std::string::npos) {
                return false;
            }

            if (preKeyQuotes > 1) {
                throw ParsingException(ExceptionMessage(g_ErrorKeyIncorrect, *pLine));
            }

            pLine->Type = Node::MapType;

            // get key
            std::string key = pLine->Data.substr(0, tokenPos);
            const size_t keyEnd = key.find_last_not_of(" \t");
            if (keyEnd == std::string::npos) {
                throw ParsingException(ExceptionMessage(g_ErrorKeyMissing, *pLine));
            }
            key.resize(keyEnd + 1);

            // handle cited key
            if (preKeyQuotes == 1) {
                if (key.front() != '"' || key.back() != '"') {
                    throw ParsingException(ExceptionMessage(g_ErrorKeyIncorrect, *pLine));
                }

                key = key.substr(1, key.size() - 2);
            }
            RemoveAllEscapeTokens(key);

            // get value
            std::string value = "";
            size_t valueStart = std::string::npos;
            if (tokenPos + 1 != pLine->Data.size()) {
                valueStart = pLine->Data.find_first_not_of(" \t", tokenPos + 1);
                if (valueStart != std::string::npos) {
                    value = pLine->Data.substr(valueStart);
                }
            }

            // make sure the value is not a sequence start
            if (isSequenceStart(value)) {
                throw ParsingException(ExceptionMessage(g_ErrorBlockSequenceNotAllowed, *pLine, valueStart));
            }

            pLine->Data = key;

            // remove all empty lines after map key
            clearTrailingEmptyLines(++it);

            // add new empty line?
            size_t newLineOffset = valueStart;
            if (newLineOffset == std::string::npos) {
                if (it != m_Lines.end() && (*it)->Offset > pLine->Offset) {
                    return true;
                }

                newLineOffset = tokenPos + 2;
            }
            else {
                newLineOffset += pLine->Offset;
            }

            // add new line with value
            unsigned char dummyBlockFlags = 0;
            if (isBlockScalar(value, pLine->No, dummyBlockFlags)) {
                newLineOffset = pLine->Offset;
            }

            ReaderLine* pNewLine = new ReaderLine(value, pLine->No, newLineOffset, Node::ScalarType);
            it = m_Lines.insert(it, pNewLine);

            // return false in order to handle next line(scalar value)
            return false;
        }

        /* Run post-processing and check for scalar. Checking for multi-line scalars. */
        void postProcessScalarLine(std::list<ReaderLine*>::iterator & it)
        {
            ReaderLine* pLine = *it;
            pLine->Type = Node::ScalarType;

            size_t parentOffset = pLine->Offset;
            if (pLine != m_Lines.front()) {
                std::list<ReaderLine*>::iterator lastIt = it;
                --lastIt;
                parentOffset = (*lastIt)->Offset;
            }

            std::list<ReaderLine*>::iterator lastNotEmpty = it++;

            // find last empty lines
            while (it != m_Lines.end()) {
                pLine = *it;
                pLine->Type = Node::ScalarType;
                if (pLine->Data.size()) {
                    if (pLine->Offset <= parentOffset) {
                        break;
                    }
                    else {
                        lastNotEmpty = it;
                    }
                }
                ++it;
            }

            clearTrailingEmptyLines(++lastNotEmpty);
        }

        /* Process root node and start of document. */
        void parseRoot(Node& root)
        {
            // get first line and start type
            auto it = m_Lines.begin();
            if (it == m_Lines.end()) {
                return;
            }

            Node::eType type = (*it)->Type;
            ReaderLine* pLine = *it;

            // handle next line
            switch (type) {
            case Node::SequenceType:
                parseSequence(root, it);
                break;
            case Node::MapType:
                parseMap(root, it);
                break;
            case Node::ScalarType:
                parseScalar(root, it);
                break;
            default:
                break;
            }

            if (it != m_Lines.end()) {
                throw InternalException(ExceptionMessage(g_ErrorUnexpectedDocumentEnd, *pLine));
            }

        }

        /* Process sequence node. */
        void parseSequence(Node& node, std::list<ReaderLine*>::iterator & it)
        {
            ReaderLine* pNextLine = nullptr;
            while (it != m_Lines.end()) {
                ReaderLine* pLine = *it;
                Node& childNode = node.push_back();

                // move to next line, error check
                ++it;
                if (it == m_Lines.end()) {
                    throw InternalException(ExceptionMessage(g_ErrorUnexpectedDocumentEnd, *pLine));
                }

                // handle value of map
                Node::eType valueType = (*it)->Type;
                switch (valueType) {
                case Node::SequenceType:
                    parseSequence(childNode, it);
                    break;
                case Node::MapType:
                    parseMap(childNode, it);
                    break;
                case Node::ScalarType:
                    parseScalar(childNode, it);
                    break;
                default:
                    break;
                }

                // check next line; if sequence and correct level, go on, else exit
                // if same level but of type map = error
                if (it == m_Lines.end() || ((pNextLine = *it)->Offset < pLine->Offset)) {
                    break;
                }

                if (pNextLine->Offset > pLine->Offset) {
                    throw ParsingException(ExceptionMessage(g_ErrorIncorrectOffset, *pNextLine));
                }

                if (pNextLine->Type != Node::SequenceType) {
                    throw InternalException(ExceptionMessage(g_ErrorDiffEntryNotAllowed, *pNextLine));
                }
            }
        }

        /* Process map node. */
        void parseMap(Node& node, std::list<ReaderLine*>::iterator & it)
        {
            ReaderLine* pNextLine = nullptr;
            while (it != m_Lines.end()) {
                ReaderLine* pLine = *it;
                Node& childNode = node[pLine->Data];

                // move to next line, error check
                ++it;
                if (it == m_Lines.end()) {
                    throw InternalException(ExceptionMessage(g_ErrorUnexpectedDocumentEnd, *pLine));
                }

                // handle value of map
                Node::eType valueType = (*it)->Type;
                switch (valueType) {
                case Node::SequenceType:
                    parseSequence(childNode, it);
                    break;
                case Node::MapType:
                    parseMap(childNode, it);
                    break;
                case Node::ScalarType:
                    parseScalar(childNode, it);
                    break;
                default:
                    break;
                }

                // check next line; if map and correct level, go on, else exit
                // if same level but of type map = error
                if (it == m_Lines.end() || ((pNextLine = *it)->Offset < pLine->Offset)) {
                    break;
                }

                if (pNextLine->Offset > pLine->Offset) {
                    throw ParsingException(ExceptionMessage(g_ErrorIncorrectOffset, *pNextLine));
                }

                if (pNextLine->Type != pLine->Type) {
                    throw InternalException(ExceptionMessage(g_ErrorDiffEntryNotAllowed, *pNextLine));
                }
            }
        }

        /* Process scalar node. */
        void parseScalar(Node & node, std::list<ReaderLine*>::iterator & it)
        {
            std::string data = "";
            ReaderLine* pFirstLine = *it;
            ReaderLine* pLine = *it;

            // check if current line is a block scalar
            unsigned char blockFlags = 0;
            bool blockScalar = isBlockScalar(pLine->Data, pLine->No, blockFlags);
            const bool newLineFlag = static_cast<bool>(blockFlags & ReaderLine::FlagMask[static_cast<size_t>(ReaderLine::ScalarNewlineFlag)]);
            const bool foldedFlag = static_cast<bool>(blockFlags & ReaderLine::FlagMask[static_cast<size_t>(ReaderLine::FoldedScalarFlag)]);
            const bool literalFlag = static_cast<bool>(blockFlags & ReaderLine::FlagMask[static_cast<size_t>(ReaderLine::LiteralScalarFlag)]);
            size_t parentOffset = 0;

            // find parent offset
            if (it != m_Lines.begin()) {
                std::list<ReaderLine*>::iterator parentIt = it;
                --parentIt;
                parentOffset = (*parentIt)->Offset;
            }

            // move to next iterator/line if current line is a block scalar
            if (blockScalar) {
                ++it;
                if (it == m_Lines.end() || (pLine = *it)->Type != Node::ScalarType) {
                    return;
                }
            }

            // not a block scalar, cut end spaces/tabs
            if (!blockScalar) {
                while (true) {
                    pLine = *it;
                    if (parentOffset != 0 && pLine->Offset <= parentOffset) {
                        throw ParsingException(ExceptionMessage(g_ErrorIncorrectOffset, *pLine));
                    }

                    const size_t endOffset = pLine->Data.find_last_not_of(" \t");
                    if (endOffset == std::string::npos) {
                        data += "\n";
                    }
                    else {
                        data += pLine->Data.substr(0, endOffset + 1);
                    }

                    // move to next line
                    ++it;
                    if (it == m_Lines.end() || (*it)->Type != Node::ScalarType) {
                        break;
                    }

                    data += " ";
                }

                if (!ValidateQuote(data)) {
                    throw ParsingException(ExceptionMessage(g_ErrorInvalidQuote, *pFirstLine));
                }
            }
            else {
                // block scalar
                pLine = *it;
                size_t blockOffset = pLine->Offset;
                if (blockOffset <= parentOffset) {
                    throw ParsingException(ExceptionMessage(g_ErrorIncorrectOffset, *pLine));
                }

                bool addedSpace = false;
                while (it != m_Lines.end() && (*it)->Type == Node::ScalarType) {
                    pLine = *it;

                    const size_t endOffset = pLine->Data.find_last_not_of(" \t");
                    if (endOffset != std::string::npos && pLine->Offset < blockOffset) {
                        throw ParsingException(ExceptionMessage(g_ErrorIncorrectOffset, *pLine));
                    }

                    if (endOffset == std::string::npos) {
                        if (addedSpace) {
                            data[data.size() - 1] = '\n';
                            addedSpace = false;
                        }
                        else {
                            data += "\n";
                        }

                        ++it;
                        continue;
                    }
                    else
                    {
                        if (blockOffset != pLine->Offset && foldedFlag) {
                            if (addedSpace) {
                                data[data.size() - 1] = '\n';
                                addedSpace = false;
                            }
                            else {
                                data += "\n";
                            }
                        }
                        data += std::string(pLine->Offset - blockOffset, ' ');
                        data += pLine->Data;
                    }

                    // move to next line
                    ++it;
                    if (it == m_Lines.end() || (*it)->Type != Node::ScalarType) {
                        if (newLineFlag) {
                            data += "\n";
                        }
                        break;
                    }

                    if (foldedFlag) {
                        data += " ";
                        addedSpace = true;
                    }
                    else if (literalFlag && endOffset != std::string::npos) {
                        data += "\n";
                    }
                }
            }

            if (data.size() && (data[0] == '"' || data[0] == '\'')) {
                data = data.substr(1, data.size() - 2);
            }

            node = data;
        }

        /*  */
        void clearLines()
        {
            for (auto it = m_Lines.begin(); it != m_Lines.end(); it++) {
                delete* it;
            }
            m_Lines.clear();
        }

        /*  */
        void clearTrailingEmptyLines(std::list<ReaderLine*>::iterator & it)
        {
            while (it != m_Lines.end()) {
                ReaderLine* pLine = *it;
                if (pLine->Data.size() == 0) {
                    delete* it;
                    it = m_Lines.erase(it);
                }
                else {
                    return;
                }

            }
        }

        /*  */
        static bool isSequenceStart(const std::string & data)
        {
            if (data.size() == 0 || data[0] != '-') {
                return false;
            }

            if (data.size() >= 2 && data[1] != ' ') {
                return false;
            }

            return true;
        }

        /*  */
        static bool isBlockScalar(const std::string & data, const size_t line, unsigned char& flags)
        {
            flags = 0;
            if (data.size() == 0) {
                return false;
            }

            if (data[0] == '|') {
                if (data.size() >= 2) {
                    if (data[1] != '-' && data[1] != ' ' && data[1] != '\t') {
                        throw ParsingException(ExceptionMessage(g_ErrorInvalidBlockScalar, line, data));
                    }
                }
                else {
                    flags |= ReaderLine::FlagMask[static_cast<size_t>(ReaderLine::ScalarNewlineFlag)];
                }
                flags |= ReaderLine::FlagMask[static_cast<size_t>(ReaderLine::LiteralScalarFlag)];
                return true;
            }

            if (data[0] == '>') {
                if (data.size() >= 2) {
                    if (data[1] != '-' && data[1] != ' ' && data[1] != '\t') {
                        throw ParsingException(ExceptionMessage(g_ErrorInvalidBlockScalar, line, data));
                    }
                }
                else {
                    flags |= ReaderLine::FlagMask[static_cast<size_t>(ReaderLine::ScalarNewlineFlag)];
                }
                flags |= ReaderLine::FlagMask[static_cast<size_t>(ReaderLine::FoldedScalarFlag)];
                return true;
            }

            return false;
        }

    public:
        std::list<ReaderLine*> m_Lines;    // List of lines.
    };

    // ---------------------------------------------------------------------------
    //  Parsing Functions
    // ---------------------------------------------------------------------------

    /* Populate given root node with deserialized data. */
    bool Parse(Node& root, const char* filename)
    {
        std::ifstream f(filename, std::ifstream::binary);
        if (!f.is_open()) {
            throw OperationException(g_ErrorCannotOpenFile);
        }

        f.seekg(0, f.end);
        size_t fileSize = static_cast<size_t>(f.tellg());
        f.seekg(0, f.beg);

        std::unique_ptr<char[]> data(new char[fileSize]);
        f.read(data.get(), fileSize);
        f.close();

        return Parse(root, data.get(), fileSize);
    }
    /* Populate given root node with deserialized data. */
    bool Parse(Node& root, std::iostream& stream)
    {
        ParseImp* pImp = nullptr;

        try
        {
            pImp = new ParseImp;
            pImp->parse(root, stream);
            delete pImp;
            return true;
        }
        catch (Exception const& e)
        {
            delete pImp;
            return false;
        }
    }
    /* Populate given root node with deserialized data. */
    bool Parse(Node& root, const std::string& string)
    {
        std::stringstream ss(string);
        return Parse(root, ss);
    }
    /* Populate given root node with deserialized data. */
    bool Parse(Node& root, const char* buffer, const size_t size)
    {
        std::stringstream ss(std::string(buffer, size));
        return Parse(root, ss);
    }

    // ---------------------------------------------------------------------------
    //  Public Class Members
    // ---------------------------------------------------------------------------

    /* Initializes a new instance of the SerializeConfig struct. */
    SerializeConfig::SerializeConfig(const size_t spaceIndentation, const size_t scalarMaxLength,
        const bool sequenceMapNewline, const bool mapScalarNewline) :
        SpaceIndentation(spaceIndentation),
        ScalarMaxLength(scalarMaxLength),
        SequenceMapNewline(sequenceMapNewline),
        MapScalarNewline(mapScalarNewline)
    {
        /* stub */
    }

    // ---------------------------------------------------------------------------
    //  Serialization Functions
    // ---------------------------------------------------------------------------

    /* Serialize node data. */
    void Serialize(const Node& root, const char* filename, const SerializeConfig& config)
    {
        std::stringstream stream;
        Serialize(root, stream, config);

        std::ofstream f(filename);
        if (!f.is_open()) {
            throw OperationException(g_ErrorCannotOpenFile);
        }

        f.write(stream.str().c_str(), stream.str().size());
        f.close();
    }

    /*  */
    size_t LineFolding(const std::string& input, std::vector<std::string>& folded, const size_t maxLength)
    {
        folded.clear();
        if (input.size() == 0) {
            return 0;
        }

        size_t currentPos = 0;
        size_t lastPos = 0;
        size_t spacePos = std::string::npos;
        while (currentPos < input.size()) {
            currentPos = lastPos + maxLength;

            if (currentPos < input.size()) {
                spacePos = input.find_first_of(' ', currentPos);
            }

            if (spacePos == std::string::npos || currentPos >= input.size()) {
                const std::string endLine = input.substr(lastPos);
                if (endLine.size()) {
                    folded.push_back(endLine);
                }

                return folded.size();
            }

            folded.push_back(input.substr(lastPos, spacePos - lastPos));

            lastPos = spacePos + 1;
        }

        return folded.size();
    }

    /*  */
    static void SerializeLoop(const Node& node, std::iostream& stream, bool useLevel, const size_t level, const SerializeConfig& config)
    {
        const size_t indention = config.SpaceIndentation;
        switch (node.type()) {
        case Node::SequenceType:
        {
            for (auto it = node.begin(); it != node.end(); it++) {
                const Node& value = (*it).second;
                if (value.isNone()) {
                    continue;
                }
                stream << std::string(level, ' ') << "- ";
                useLevel = false;
                if (value.isSequence() || (value.isMap() && config.SequenceMapNewline)) {
                    useLevel = true;
                    stream << "\n";
                }

                SerializeLoop(value, stream, useLevel, level + 2, config);
            }

        }
        break;
        case Node::MapType:
        {
            size_t count = 0;
            for (auto it = node.begin(); it != node.end(); it++) {
                const Node& value = (*it).second;
                if (value.isNone()) {
                    continue;
                }

                if (useLevel || count > 0) {
                    stream << std::string(level, ' ');
                }

                std::string key = (*it).first;
                AddEscapeTokens(key, "\\\"");
                if (ShouldBeCited(key)) {
                    stream << "\"" << key << "\"" << ": ";
                }
                else {
                    stream << key << ": ";
                }


                useLevel = false;
                if (!value.isScalar() || (value.isScalar() && config.MapScalarNewline)) {
                    useLevel = true;
                    stream << "\n";
                }

                SerializeLoop(value, stream, useLevel, level + indention, config);

                useLevel = true;
                count++;
            }

        }
        break;
        case Node::ScalarType:
        {
            const std::string value = node.as<std::string>();

            // empty scalar
            if (value.size() == 0) {
                stream << "\n";
                break;
            }

            // get lines of scalar
            std::string line = "";
            std::vector<std::string> lines;
            std::istringstream iss(value);
            while (!iss.eof()) {
                std::getline(iss, line);
                lines.push_back(line);
            }

            // block scalar
            const std::string& lastLine = lines.back();
            const bool endNewline = lastLine.size() == 0;
            if (endNewline) {
                lines.pop_back();
            }

            // literal
            if (lines.size() > 1) {
                stream << "|";
            }
            // folded/plain
            else {
                const std::string frontLine = lines.front();
                if (config.ScalarMaxLength == 0 || lines.front().size() <= config.ScalarMaxLength ||
                    LineFolding(frontLine, lines, config.ScalarMaxLength) == 1) {
                    if (useLevel) {
                        stream << std::string(level, ' ');
                    }

                    if (ShouldBeCited(value)) {
                        stream << "\"" << value << "\"\n";
                        break;
                    }
                    stream << value << "\n";
                    break;
                }
                else {
                    stream << ">";
                }
            }

            if (!endNewline) {
                stream << "-";
            }
            stream << "\n";

            for (auto it = lines.begin(); it != lines.end(); it++) {
                stream << std::string(level, ' ') << (*it) << "\n";
            }
        }
        break;

        default:
            break;
        }
    }

    /* Serialize node data. */
    void Serialize(const Node& root, std::iostream& stream, const SerializeConfig& config)
    {
        if (config.SpaceIndentation < 2) {
            throw OperationException(g_ErrorIndentation);
        }

        SerializeLoop(root, stream, false, 0, config);
    }

    /* Serialize node data. */
    void Serialize(const Node& root, std::string& string, const SerializeConfig& config)
    {
        std::stringstream stream;
        Serialize(root, stream, config);
        string = stream.str();
    }

    // ---------------------------------------------------------------------------
    //  Global Functions
    // ---------------------------------------------------------------------------

    /**
     * @brief Returns an exception message for a reader line with the given message.
     * @param message Error message.
     * @param line ReaderLine instance.
     * @returns std::string Compiled exception meessage.
     */
    std::string ExceptionMessage(const std::string& message, ReaderLine& line) { return message + std::string(" Line ") + std::to_string(line.No) + std::string(": ") + line.Data; }
    /**
     * @brief Returns an exception message for a reader line with the given message.
     * @param message Error message.
     * @param line ReaderLine instance.
     * @param errorPos Error position.
     * @returns std::string Compiled exception meessage.
     */
    std::string ExceptionMessage(const std::string& message, ReaderLine& line, const size_t errorPos) { return message + std::string(" Line ") + std::to_string(line.No) + std::string(" column ") + std::to_string(errorPos + 1) + std::string(": ") + line.Data; }
    /**
     * @brief Returns an exception message for a reader line with the given message.
     * @param message Error message.
     * @param size_t Line number error occurred.
     * @param errorPos Error position.
     * @returns std::string Compiled exception meessage.
     */
    std::string ExceptionMessage(const std::string& message, const size_t errorLine, const size_t errorPos) { return message + std::string(" Line ") + std::to_string(errorLine) + std::string(" column ") + std::to_string(errorPos); }
    /**
     * @brief Returns an exception message for a reader line with the given message.
     * @param message Error message.
     * @param size_t Line number error occurred.
     * @param data Error data.
     * @returns std::string Compiled exception meessage.
     */
    std::string ExceptionMessage(const std::string& message, const size_t errorLine, const std::string& data) { return message + std::string(" Line ") + std::to_string(errorLine) + std::string(": ") + data; }

    /*  */
    bool FindQuote(const std::string& input, size_t& start, size_t& end, size_t searchPos)
    {
        start = end = std::string::npos;
        size_t qPos = searchPos;
        bool foundStart = false;

        while (qPos != std::string::npos) {
            // find first quote
            qPos = input.find_first_of("\"'", qPos);
            if(qPos == std::string::npos) {
                return false;
            }

            const char token = input[qPos];
            if (token == '"' && (qPos == 0 || input[qPos-1] != '\\')) {
                // found start quote
                if (!foundStart) {
                    start = qPos;
                    foundStart = true;
                }
                // found end quote
                else {
                    end = qPos;
                    return true;
                }
            }

            // check if it's possible for another loop
            if (qPos + 1 == input.size()) {
                return false;
            }
            qPos++;
        }

        return false;
    }

    /*  */
    size_t FindNotCited(const std::string& input, char token, size_t& preQuoteCount)
    {
        preQuoteCount = 0;
        size_t tokenPos = input.find_first_of(token);
        if (tokenPos == std::string::npos) {
            return std::string::npos;
        }

        // find all quotes
        std::vector<std::pair<size_t, size_t>> quotes;

        size_t quoteStart = 0;
        size_t quoteEnd = 0;
        while (FindQuote(input, quoteStart, quoteEnd, quoteEnd)) {
            quotes.push_back({quoteStart, quoteEnd});

            if (quoteEnd + 1 == input.size()) {
                break;
            }
            quoteEnd++;
        }

        if (quotes.size() == 0) {
            return tokenPos;
        }

        size_t currentQuoteIndex = 0;
        std::pair<size_t, size_t> currentQuote = {0, 0};
        while (currentQuoteIndex < quotes.size()) {
            currentQuote = quotes[currentQuoteIndex];

            if (tokenPos < currentQuote.first) {
                return tokenPos;
            }

            preQuoteCount++;
            if (tokenPos <= currentQuote.second) {
                // find next token
                if (tokenPos + 1 == input.size()) {
                    return std::string::npos;
                }

                tokenPos = input.find_first_of(token, tokenPos + 1);
                if (tokenPos == std::string::npos) {
                    return std::string::npos;
                }
            }

            currentQuoteIndex++;
        }

        return tokenPos;
    }

    /*  */
    size_t FindNotCited(const std::string& input, char token)
    {
        size_t dummy = 0;
        return FindNotCited(input, token, dummy);
    }

    /*  */
    bool ValidateQuote(const std::string& input)
    {
        if (input.size() == 0) {
            return true;
        }

        char token = 0;
        size_t searchPos = 0;
        if (input[0] == '\"' || input[0] == '\'') {
            if (input.size() == 1) {
                return false;
            }
            token = input[0];
            searchPos = 1;
        }

        while (searchPos != std::string::npos && searchPos < input.size() - 1) {
            searchPos = input.find_first_of("\"'", searchPos + 1);
            if(searchPos == std::string::npos) {
                break;
            }

            const char foundToken = input[searchPos];

            if (input[searchPos] == '\"' || input[searchPos] == '\'') {
                if (token == 0 && input[searchPos-1] != '\\') {
                    return false;
                }

                if (foundToken == token && input[searchPos-1] != '\\') {
                    if(searchPos == input.size() - 1)
                    {
                        return true;
                    }
                    return false;
                }
            }
        }

        return token == 0;
    }

    /*  */
    void CopyNode(const Node& from, Node& to)
    {
        const Node::eType type = from.type();

        switch(type) {
        case Node::SequenceType:
            for (auto it = from.begin(); it != from.end(); it++) {
                const Node & currentNode = (*it).second;
                Node & newNode = to.push_back();
                CopyNode(currentNode, newNode);
            }
            break;
        case Node::MapType:
            for (auto it = from.begin(); it != from.end(); it++) {
                const Node & currentNode = (*it).second;
                Node & newNode = to[(*it).first];
                CopyNode(currentNode, newNode);
            }
            break;
        case Node::ScalarType:
            to = from.as<std::string>();
            break;
        case Node::None:
            break;
        }
    }

    /*  */
    bool ShouldBeCited(const std::string& key) { return key.find_first_of("\":{}[],&*#?|-<>=!%@") != std::string::npos; }

    /*  */
    void AddEscapeTokens(std::string& input, const std::string& tokens)
    {
        for (auto it = tokens.begin(); it != tokens.end(); it++) {
            const char token = *it;
            const std::string replace = std::string("\\") + std::string(1, token);
            size_t found = input.find_first_of(token);
            while (found != std::string::npos) {
                input.replace(found, 1, replace);
                found = input.find_first_of(token, found + 2);
            }
        }
    }

    /*  */
    void RemoveAllEscapeTokens(std::string & input)
    {
        size_t found = input.find_first_of("\\");
        while (found != std::string::npos) {
            if (found + 1 == input.size()) {
                return;
            }

            std::string replace(1, input[found + 1]);
            input.replace(found, 2, replace);
            found = input.find_first_of("\\", found + 1);
        }
    }
} // namespace yaml
