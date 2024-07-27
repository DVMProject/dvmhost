/*
 * Project 25 IMBE Encoder/Decoder Fixed-Point implementation
 * Developed by Pavel Yazev E-mail: pyazev@gmail.com
 * Version 1.0 (c) Copyright 2009
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * The software is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Boston, MA
 * 02110-1301, USA.
 */
#ifndef __CH_ENCODE_H__
#define __CH_ENCODE_H__

// ---------------------------------------------------------------------------
//	 Macros
// ---------------------------------------------------------------------------

#define EN_BIT_STREAM_LEN  (3 + 3*12 + 6 + 2*11 + 3)

// ---------------------------------------------------------------------------
//	 Global Functions
// ---------------------------------------------------------------------------

void encode_frame_vector(IMBE_PARAM *imbe_param, Word16 *frame_vector);

#endif // __CH_ENCODE_H__
