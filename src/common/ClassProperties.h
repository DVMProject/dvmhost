// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ClassProperties.h
 * @ingroup common
 */
#pragma once
#if !defined(__CLASS_PROPERTIES_H__)
#define __CLASS_PROPERTIES_H__

#if !defined(__COMMON_DEFINES_H__)
#warning "ClassProperties.h included before Defines.h, please check include order"
#include "common/Defines.h"
#endif

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

/**
 * @addtogroup common
 * @{
 */

/**
 * Class Copy Code Pattern
 */

/**
 * @brief Declare a private copy implementation.
 *   This requires the copy(const type& data) to be declared in the class definition.
 * @ingroup common
 * @param type Atomic type.
 */
#define DECLARE_COPY(type)                                                                      \
        private: virtual void copy(const type& data);                                           \
        public: __forceinline type& operator=(const type& data) {                               \
            if (this != &data) {                                                                \
                copy(data);                                                                     \
            }                                                                                   \
            return *this;                                                                       \
        }
/**
 * @brief Declare a protected copy implementation.
 *   This requires the copy(const type& data) to be declared in the class definition.
 * @ingroup common
 * @param type Atomic type.
 */
#define DECLARE_PROTECTED_COPY(type)                                                            \
        protected: virtual void copy(const type& data);                                         \
        public: __forceinline type& operator=(const type& data) {                               \
            if (this != &data) {                                                                \
                copy(data);                                                                     \
            }                                                                                   \
            return *this;                                                                       \
        }

/**
 * Property Declaration
 *  These macros should always be used LAST in a "public" section of a class definition.
 */

/**
 * @brief Declare a read-only get property.
 *  This creates a "property" that is read-only and can only be set internally to the class. Properties created
 *  with this macro generate an internal variable with the name "m_<variableName>", and a getter method
 *  with the name "get<propertyName>()". The getter method is inline and returns the value of the internal
 *  variable. The internal variable is private, and the getter method is public.
 * @ingroup common
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 * @param propName Property name.
 */
#define DECLARE_RO_PROPERTY(type, variableName, propName)                                       \
        private: type m_##variableName;                                                         \
        public: __forceinline type get##propName(void) const { return m_##variableName; }
/**
 * @brief Declare a read-only get property.
 *  This creates a "property" that is read-only and can only be set internally to the class. Properties created
 *  with this macro generate an internal variable with the name "m_<variableName>", and a getter method
 *  with the name "get<propertyName>()". The getter method is inline and returns the value of the internal
 *  variable. The internal variable is protected, and the getter method is public.
 * @ingroup common
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 * @param propName Property name.
 */
#define DECLARE_PROTECTED_RO_PROPERTY(type, variableName, propName)                             \
        protected: type m_##variableName;                                                       \
        public: __forceinline type get##propName(void) const { return m_##variableName; }

/**
 * @brief Declare a read-only property, does not use "get" prefix for getter.
 *  This creates a "property" that is read-only and can only be set internally to the class. Properties created
 *  with this macro generate an internal variable with the name "m_<variableName>", and a getter method
 *  with the name "<variableName>()". The getter method is inline and returns the value of the internal
 *  variable. The internal variable is private, and the getter method is public.
 * @ingroup common
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 */
#define DECLARE_PROTECTED_RO_PROPERTY_PLAIN(type, variableName)                                 \
        protected: type m_##variableName;                                                       \
        public: __forceinline type variableName(void) const { return m_##variableName; }
/**
 * @brief Declare a read-only get property, does not use "get" prefix for getter.
 *  This creates a "property" that is read-only and can only be set internally to the class. Properties created
 *  with this macro generate an internal variable with the name "m_<variableName>", and a getter method
 *  with the name "<variableName>()". The getter method is inline and returns the value of the internal
 *  variable. The internal variable is private, and the getter method is public.
 * @ingroup common
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 */
#define DECLARE_RO_PROPERTY_PLAIN(type, variableName)                                           \
        private: type m_##variableName;                                                         \
        public: __forceinline type variableName(void) const { return m_##variableName; }

/**
 * @brief Declare a get and set private property.
 *  This creates a "property" that is read/write. Properties created with this macro generate an internal variable
 *  with the name "m_<variableName>", a getter method with the name "get<propertyName>()", and a setter method
 *  with the name "set<propertyName>(value)". The getter method is inline and returns the value of the internal variable.
 *  The internal variable is private, and the getter and setter methods are public.
 * @ingroup common
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 * @param propName Property name.
 */
#define DECLARE_PROPERTY(type, variableName, propName)                                          \
        private: type m_##variableName;                                                         \
        public: __forceinline type get##propName(void) const { return m_##variableName; }       \
                __forceinline void set##propName(type val) { m_##variableName = val; }
/**
 * @brief Declare a get and set protected property.
 *  This creates a "property" that is read/write. Properties created with this macro generate an internal variable
 *  with the name "m_<variableName>", a getter method with the name "get<propertyName>()", and a setter method
 *  with the name "set<propertyName>(value)". The getter method is inline and returns the value of the internal variable.
 *  The internal variable is protected, and the getter and setter methods are public.
 * @ingroup common
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 * @param propName Property name.
 */
#define DECLARE_PROTECTED_PROPERTY(type, variableName, propName)                                \
        protected: type m_##variableName;                                                       \
        public: __forceinline type get##propName(void) const { return m_##variableName; }       \
                __forceinline void set##propName(type val) { m_##variableName = val; }

/**
 * @brief Declare a private property, does not use "get"/"set" prefix.
 *  This creates a "property" that is read/write. Properties created with this macro generate an internal variable
 *  with the name "m_<variableName>", and a getter method with the name "<variableName>()", and a setter method
 *  "<propertyName>(value)". The getter method is inline and returns the value of the internal variable. The internal 
 *  variable is private, and the getter and setter methods are public.
 * @ingroup common
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 */
#define DECLARE_PROPERTY_PLAIN(type, variableName)                                              \
        private: type m_##variableName;                                                         \
        public: __forceinline type variableName(void) const { return m_##variableName; }        \
                __forceinline void variableName(type val) { m_##variableName = val; }
/**
 * @brief Declare a protected property, does not use "get"/"set" prefix.
 *  This creates a "property" that is read/write. Properties created with this macro generate an internal variable
 *  with the name "m_<variableName>", and a getter method with the name "<variableName>()", and a setter method
 *  "<propertyName>(value)". The getter method is inline and returns the value of the internal variable. The internal
 *  variable is protected, and the getter and setter methods are public.
 * @ingroup common
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 */
#define DECLARE_PROTECTED_PROPERTY_PLAIN(type, variableName)                                    \
        protected: type m_##variableName;                                                       \
        public: __forceinline type variableName(void) const { return m_##variableName; }        \
                __forceinline void variableName(type val) { m_##variableName = val; }

/** @} */

#endif // __CLASS_PROPERTIES_H__