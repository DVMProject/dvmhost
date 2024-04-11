// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2018,2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace edac;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t CRC8_TABLE[] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31,
    0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9,
    0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1,
    0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE,
    0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16,
    0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80,
    0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8,
    0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10,
    0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F,
    0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7,
    0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF,
    0xFA, 0xFD, 0xF4, 0xF3, 0x01 };

const uint16_t CCITT16_TABLE1[] = {
    0x0000U, 0x1189U, 0x2312U, 0x329BU, 0x4624U, 0x57ADU, 0x6536U, 0x74BFU,
    0x8C48U, 0x9DC1U, 0xAF5AU, 0xBED3U, 0xCA6CU, 0xDBE5U, 0xE97EU, 0xF8F7U,
    0x1081U, 0x0108U, 0x3393U, 0x221AU, 0x56A5U, 0x472CU, 0x75B7U, 0x643EU,
    0x9CC9U, 0x8D40U, 0xBFDBU, 0xAE52U, 0xDAEDU, 0xCB64U, 0xF9FFU, 0xE876U,
    0x2102U, 0x308BU, 0x0210U, 0x1399U, 0x6726U, 0x76AFU, 0x4434U, 0x55BDU,
    0xAD4AU, 0xBCC3U, 0x8E58U, 0x9FD1U, 0xEB6EU, 0xFAE7U, 0xC87CU, 0xD9F5U,
    0x3183U, 0x200AU, 0x1291U, 0x0318U, 0x77A7U, 0x662EU, 0x54B5U, 0x453CU,
    0xBDCBU, 0xAC42U, 0x9ED9U, 0x8F50U, 0xFBEFU, 0xEA66U, 0xD8FDU, 0xC974U,
    0x4204U, 0x538DU, 0x6116U, 0x709FU, 0x0420U, 0x15A9U, 0x2732U, 0x36BBU,
    0xCE4CU, 0xDFC5U, 0xED5EU, 0xFCD7U, 0x8868U, 0x99E1U, 0xAB7AU, 0xBAF3U,
    0x5285U, 0x430CU, 0x7197U, 0x601EU, 0x14A1U, 0x0528U, 0x37B3U, 0x263AU,
    0xDECDU, 0xCF44U, 0xFDDFU, 0xEC56U, 0x98E9U, 0x8960U, 0xBBFBU, 0xAA72U,
    0x6306U, 0x728FU, 0x4014U, 0x519DU, 0x2522U, 0x34ABU, 0x0630U, 0x17B9U,
    0xEF4EU, 0xFEC7U, 0xCC5CU, 0xDDD5U, 0xA96AU, 0xB8E3U, 0x8A78U, 0x9BF1U,
    0x7387U, 0x620EU, 0x5095U, 0x411CU, 0x35A3U, 0x242AU, 0x16B1U, 0x0738U,
    0xFFCFU, 0xEE46U, 0xDCDDU, 0xCD54U, 0xB9EBU, 0xA862U, 0x9AF9U, 0x8B70U,
    0x8408U, 0x9581U, 0xA71AU, 0xB693U, 0xC22CU, 0xD3A5U, 0xE13EU, 0xF0B7U,
    0x0840U, 0x19C9U, 0x2B52U, 0x3ADBU, 0x4E64U, 0x5FEDU, 0x6D76U, 0x7CFFU,
    0x9489U, 0x8500U, 0xB79BU, 0xA612U, 0xD2ADU, 0xC324U, 0xF1BFU, 0xE036U,
    0x18C1U, 0x0948U, 0x3BD3U, 0x2A5AU, 0x5EE5U, 0x4F6CU, 0x7DF7U, 0x6C7EU,
    0xA50AU, 0xB483U, 0x8618U, 0x9791U, 0xE32EU, 0xF2A7U, 0xC03CU, 0xD1B5U,
    0x2942U, 0x38CBU, 0x0A50U, 0x1BD9U, 0x6F66U, 0x7EEFU, 0x4C74U, 0x5DFDU,
    0xB58BU, 0xA402U, 0x9699U, 0x8710U, 0xF3AFU, 0xE226U, 0xD0BDU, 0xC134U,
    0x39C3U, 0x284AU, 0x1AD1U, 0x0B58U, 0x7FE7U, 0x6E6EU, 0x5CF5U, 0x4D7CU,
    0xC60CU, 0xD785U, 0xE51EU, 0xF497U, 0x8028U, 0x91A1U, 0xA33AU, 0xB2B3U,
    0x4A44U, 0x5BCDU, 0x6956U, 0x78DFU, 0x0C60U, 0x1DE9U, 0x2F72U, 0x3EFBU,
    0xD68DU, 0xC704U, 0xF59FU, 0xE416U, 0x90A9U, 0x8120U, 0xB3BBU, 0xA232U,
    0x5AC5U, 0x4B4CU, 0x79D7U, 0x685EU, 0x1CE1U, 0x0D68U, 0x3FF3U, 0x2E7AU,
    0xE70EU, 0xF687U, 0xC41CU, 0xD595U, 0xA12AU, 0xB0A3U, 0x8238U, 0x93B1U,
    0x6B46U, 0x7ACFU, 0x4854U, 0x59DDU, 0x2D62U, 0x3CEBU, 0x0E70U, 0x1FF9U,
    0xF78FU, 0xE606U, 0xD49DU, 0xC514U, 0xB1ABU, 0xA022U, 0x92B9U, 0x8330U,
    0x7BC7U, 0x6A4EU, 0x58D5U, 0x495CU, 0x3DE3U, 0x2C6AU, 0x1EF1U, 0x0F78U };

const uint16_t CCITT16_TABLE2[] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0 };

const uint32_t CRC32_TABLE[] = {
    0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
    0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
    0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
    0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
    0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
    0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
    0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
    0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
    0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072,
    0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
    0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
    0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
    0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
    0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
    0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
    0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
    0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
    0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
    0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
    0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
    0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
    0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
    0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
    0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
    0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
    0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
    0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
    0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
    0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
    0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
    0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
    0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4 };

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Check 5-bit CRC.
/// </summary>
/// <param name="in">Boolean bit array.</param>
/// <param name="tcrc">Computed CRC to check.</param>
/// <returns>True, if CRC is valid, otherwise false.</returns>
bool CRC::checkFiveBit(bool* in, uint32_t tcrc)
{
    assert(in != nullptr);

    uint32_t crc;
    encodeFiveBit(in, crc);

    return crc == tcrc;
}

/// <summary>
/// Encode 5-bit CRC.
/// </summary>
/// <param name="in">Boolean bit array.</param>
/// <param name="tcrc">Computed CRC.</param>
void CRC::encodeFiveBit(const bool* in, uint32_t& tcrc)
{
    assert(in != nullptr);

    uint16_t total = 0U;
    for (uint32_t i = 0U; i < 72U; i += 8U) {
        uint8_t c;
        Utils::bitsToByteBE(in + i, c);
        total += c;
    }

    total %= 31U;

    tcrc = total;
}

/// <summary>
/// Check 16-bit CRC CCITT-162.
/// </summary>
/// <remarks>This uses polynomial 0x1021.</remarks>
/// <param name="in">Input byte array.</param>
/// <param name="length">Length of byte array.</param>
/// <returns>True, if CRC is valid, otherwise false.</returns>
bool CRC::checkCCITT162(const uint8_t *in, uint32_t length)
{
    assert(in != nullptr);
    assert(length > 2U);

    union {
        uint16_t crc16;
        uint8_t  crc8[2U];
    };

    crc16 = 0U;

    for (unsigned i = 0U; i < (length - 2U); i++)
        crc16 = (uint16_t(crc8[0U]) << 8) ^ CCITT16_TABLE2[crc8[1U] ^ in[i]];

    crc16 = ~crc16;

#if DEBUG_CRC_CHECK
    uint16_t inCrc = (in[length - 2U] << 8) | (in[length - 1U] << 0);
    LogDebug(LOG_HOST, "CRC::checkCCITT162(), crc = $%04X, in = $%04X, len = %u", crc16, inCrc, length);
#endif

    return crc8[0U] == in[length - 1U] && crc8[1U] == in[length - 2U];
}

/// <summary>
/// Encode 16-bit CRC CCITT-162.
/// </summary>
/// <remarks>This uses polynomial 0x1021.</remarks>
/// <param name="in">Input byte array.</param>
/// <param name="length">Length of byte array.</param>
void CRC::addCCITT162(uint8_t* in, uint32_t length)
{
    assert(in != nullptr);
    assert(length > 2U);

    union {
        uint16_t crc16;
        uint8_t  crc8[2U];
    };

    crc16 = 0U;

    for (unsigned i = 0U; i < (length - 2U); i++)
        crc16 = (uint16_t(crc8[0U]) << 8) ^ CCITT16_TABLE2[crc8[1U] ^ in[i]];

    crc16 = ~crc16;

#if DEBUG_CRC_ADD
    LogDebug(LOG_HOST, "CRC::addCCITT162(), crc = $%04X, len = %u", crc16, length);
#endif

    in[length - 1U] = crc8[0U];
    in[length - 2U] = crc8[1U];
}

/// <summary>
/// Check 16-bit CRC CCITT-161.
/// </summary>
/// <remarks>This uses polynomial 0x1189.</remarks>
/// <param name="in">Input byte array.</param>
/// <param name="length">Length of byte array.</param>
/// <returns>True, if CRC is valid, otherwise false.</returns>
bool CRC::checkCCITT161(const uint8_t *in, uint32_t length)
{
    assert(in != nullptr);
    assert(length > 2U);

    union {
        uint16_t crc16;
        uint8_t  crc8[2U];
    };

    crc16 = 0xFFFFU;

    for (uint32_t i = 0U; i < (length - 2U); i++)
        crc16 = uint16_t(crc8[1U]) ^ CCITT16_TABLE1[crc8[0U] ^ in[i]];

    crc16 = ~crc16;

#if DEBUG_CRC_CHECK
    uint16_t inCrc = (in[length - 2U] << 8) | (in[length - 1U] << 0);
    LogDebug(LOG_HOST, "CRC::checkCCITT161(), crc = $%04X, in = $%04X, len = %u", crc16, inCrc, length);
#endif

    return crc8[0U] == in[length - 2U] && crc8[1U] == in[length - 1U];
}

/// <summary>
/// Encode 16-bit CRC CCITT-161.
/// </summary>
/// <remarks>This uses polynomial 0x1189.</remarks>
/// <param name="in">Input byte array.</param>
/// <param name="length">Length of byte array.</param>
void CRC::addCCITT161(uint8_t* in, uint32_t length)
{
    assert(in != nullptr);
    assert(length > 2U);

    union {
        uint16_t crc16;
        uint8_t  crc8[2U];
    };

    crc16 = 0xFFFFU;

    for (uint32_t i = 0U; i < (length - 2U); i++)
        crc16 = uint16_t(crc8[1U]) ^ CCITT16_TABLE1[crc8[0U] ^ in[i]];

    crc16 = ~crc16;

#if DEBUG_CRC_ADD
    LogDebug(LOG_HOST, "CRC::addCCITT161(), crc = $%04X, len = %u", crc16, length);
#endif

    in[length - 2U] = crc8[0U];
    in[length - 1U] = crc8[1U];
}

/// <summary>
/// Check 32-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="length">Length of byte array.</param>
/// <returns>True, if CRC is valid, otherwise false.</returns>
bool CRC::checkCRC32(const uint8_t *in, uint32_t length)
{
    assert(in != nullptr);
    assert(length > 4U);

    union {
        uint32_t crc32;
        uint8_t  crc8[4U];
    };

    uint32_t i = 0;
    crc32 = 0x00000000U;

    for (uint32_t j = (length - 4U); j-- > 0; i++) {
        uint32_t idx = ((crc32 >> 24) ^ in[i]) & 0xFFU;
        crc32 = (CRC32_TABLE[idx] ^ (crc32 << 8)) & 0xFFFFFFFFU;
    }

    crc32 = ~crc32;
    crc32 &= 0xFFFFFFFFU;

#if DEBUG_CRC_CHECK
    uint32_t inCrc = (in[length - 4U] << 24) | (in[length - 3U] << 16) | (in[length - 2U] << 8) | (in[length - 1U] << 0);
    LogDebug(LOG_HOST, "CRC::checkCRC32(), crc = $%08X, in = $%08X, len = %u", crc32, inCrc, length);
#endif

    return crc8[0U] == in[length - 1U] && crc8[1U] == in[length - 2U] && crc8[2U] == in[length - 3U] && crc8[3U] == in[length - 4U];
}

/// <summary>
/// Encode 32-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="length">Length of byte array.</param>
void CRC::addCRC32(uint8_t* in, uint32_t length)
{
    assert(in != nullptr);
    assert(length > 4U);

    union {
        uint32_t crc32;
        uint8_t  crc8[4U];
    };

    uint32_t i = 0;
    crc32 = 0x00000000U;

    for (uint32_t j = (length - 4U); j-- > 0; i++) {
        uint32_t idx = ((crc32 >> 24) ^ in[i]) & 0xFFU;
        crc32 = (CRC32_TABLE[idx] ^ (crc32 << 8)) & 0xFFFFFFFFU;
    }

    crc32 = ~crc32;
    crc32 &= 0xFFFFFFFFU;

#if DEBUG_CRC_ADD
    LogDebug(LOG_HOST, "CRC::addCRC32(), crc = $%08X, len = %u", crc32, length);
#endif

    in[length - 1U] = crc8[0U];
    in[length - 2U] = crc8[1U];
    in[length - 3U] = crc8[2U];
    in[length - 4U] = crc8[3U];
}

/// <summary>
/// Generate 8-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="length">Length of byte array.</param>
/// <returns>Calculated 8-bit CRC value.</returns>
uint8_t CRC::crc8(const uint8_t *in, uint32_t length)
{
    assert(in != nullptr);

    uint8_t crc = 0U;

    for (uint32_t i = 0U; i < length; i++)
        crc = CRC8_TABLE[crc ^ in[i]];

#if DEBUG_CRC_CHECK
    LogDebug(LOG_HOST, "CRC::crc8(), crc = $%02X, len = %u", crc, length);
#endif

    return crc;
}

/// <summary>
/// Check 6-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns>True, if CRC is valid, otherwise false.</returns>
bool CRC::checkCRC6(const uint8_t* in, uint32_t bitLength)
{
    assert(in != nullptr);

    uint8_t crc = createCRC6(in, bitLength);

    uint8_t temp[1U];
    temp[0U] = 0x00U;
    uint32_t j = bitLength;
    for (uint32_t i = 2U; i < 8U; i++, j++) {
        bool b = READ_BIT(in, j);
        WRITE_BIT(temp, i, b);
    }

#if DEBUG_CRC_CHECK
    uint32_t inCrc = temp[0U];
    LogDebug(LOG_HOST, "CRC::checkCRC6(), crc = $%04X, in = $%04X, bitlen = %u", crc, inCrc, bitLength);
#endif

    return crc == temp[0U];
}

/// <summary>
/// Encode 6-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns>6-bit CRC.</returns>
uint8_t CRC::addCRC6(uint8_t* in, uint32_t bitLength)
{
    assert(in != nullptr);

    uint8_t crc[1U];
    crc[0U] = createCRC6(in, bitLength);

    uint32_t n = bitLength;
    for (uint32_t i = 2U; i < 8U; i++, n++) {
        bool b = READ_BIT(crc, i);
        WRITE_BIT(in, n, b);
    }

#if DEBUG_CRC_ADD
    LogDebug(LOG_HOST, "CRC::addCRC6(), crc = $%04X, bitlen = %u", crc[0U], bitLength);
#endif
    return crc[0U];
}

/// <summary>
/// Check 12-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns>True, if CRC is valid, otherwise false.</returns>
bool CRC::checkCRC12(const uint8_t* in, uint32_t bitLength)
{
    assert(in != nullptr);

    uint16_t crc = createCRC12(in, bitLength);
    uint8_t temp1[2U];
    temp1[0U] = (crc >> 8) & 0xFFU;
    temp1[1U] = (crc >> 0) & 0xFFU;

    uint8_t temp2[2U];
    temp2[0U] = 0x00U;
    temp2[1U] = 0x00U;
    uint32_t j = bitLength;
    for (uint32_t i = 4U; i < 16U; i++, j++) {
        bool b = READ_BIT(in, j);
        WRITE_BIT(temp2, i, b);
    }

#if DEBUG_CRC_CHECK
    uint16_t inCrc = (temp2[0U] << 8) | (temp2[1U] << 0);
    LogDebug(LOG_HOST, "CRC:checkCRC12(), crc = $%04X, in = $%04X, bitlen = %u", crc, inCrc, bitLength);
#endif

    return temp1[0U] == temp2[0U] && temp1[1U] == temp2[1U];
}

/// <summary>
/// Encode 12-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns>12-bit CRC.</returns>
uint16_t CRC::addCRC12(uint8_t* in, uint32_t bitLength)
{
    assert(in != nullptr);

    uint16_t crc = createCRC12(in, bitLength);

    uint8_t temp[2U];
    temp[0U] = (crc >> 8) & 0xFFU;
    temp[1U] = (crc >> 0) & 0xFFU;

    uint32_t n = bitLength;
    for (uint32_t i = 4U; i < 16U; i++, n++) {
        bool b = READ_BIT(temp, i);
        WRITE_BIT(in, n, b);
    }

#if DEBUG_CRC_ADD
    LogDebug(LOG_HOST, "CRC::addCRC12(), crc = $%04X, bitlen = %u", crc, bitLength);
#endif
    return crc;
}

/// <summary>
/// Check 15-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns>True, if CRC is valid, otherwise false.</returns>
bool CRC::checkCRC15(const uint8_t* in, uint32_t bitLength)
{
    assert(in != nullptr);

    uint16_t crc = createCRC15(in, bitLength);
    uint8_t temp1[2U];
    temp1[0U] = (crc >> 8) & 0xFFU;
    temp1[1U] = (crc >> 0) & 0xFFU;

    uint8_t temp2[2U];
    temp2[0U] = 0x00U;
    temp2[1U] = 0x00U;
    uint32_t j = bitLength;
    for (uint32_t i = 1U; i < 16U; i++, j++) {
        bool b = READ_BIT(in, j);
        WRITE_BIT(temp2, i, b);
    }

#if DEBUG_CRC_CHECK
    uint16_t inCrc = (temp2[0U] << 8) | (temp2[1U] << 0);
    LogDebug(LOG_HOST, "CRC:checkCRC15(), crc = $%04X, in = $%04X, bitlen = %u", crc, inCrc, bitLength);
#endif

    return temp1[0U] == temp2[0U] && temp1[1U] == temp2[1U];
}

/// <summary>
/// Encode 15-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns>15-bit CRC.</returns>
uint16_t CRC::addCRC15(uint8_t* in, uint32_t bitLength)
{
    assert(in != nullptr);

    uint16_t crc = createCRC15(in, bitLength);

    uint8_t temp[2U];
    temp[0U] = (crc >> 8) & 0xFFU;
    temp[1U] = (crc >> 0) & 0xFFU;

    uint32_t n = bitLength;
    for (uint32_t i = 1U; i < 16U; i++, n++) {
        bool b = READ_BIT(temp, i);
        WRITE_BIT(in, n, b);
    }

#if DEBUG_CRC_ADD
    LogDebug(LOG_HOST, "CRC::addCRC15(), crc = $%04X, bitlen = %u", crc, bitLength);
#endif
    return crc;
}

/// <summary>
/// Check 16-bit CRC CCITT-162 w/ initial generator of 1.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns>True, if CRC is valid, otherwise false.</returns>
bool CRC::checkCRC16(const uint8_t* in, uint32_t bitLength)
{
    assert(in != nullptr);

    uint16_t crc = createCRC16(in, bitLength);
    uint8_t temp1[2U];
    temp1[0U] = (crc >> 8) & 0xFFU;
    temp1[1U] = (crc >> 0) & 0xFFU;

    uint8_t temp2[2U];
    temp2[0U] = 0x00U;
    temp2[1U] = 0x00U;
    uint32_t j = bitLength;
    for (uint32_t i = 0U; i < 16U; i++, j++) {
        bool b = READ_BIT(in, j);
        WRITE_BIT(temp2, i, b);
    }

#if DEBUG_CRC_CHECK
    uint16_t inCrc = (temp2[0U] << 8) | (temp2[1U] << 0);
    LogDebug(LOG_HOST, "CRC:checkCRC16(), crc = $%04X, in = $%04X, bitlen = %u", crc, inCrc, bitLength);
#endif

    return temp1[0U] == temp2[0U] && temp1[1U] == temp2[1U];
}

/// <summary>
/// Encode 16-bit CRC CCITT-162 w/ initial generator of 1.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <param name="offset">Offset in bits to write CRC.</param>
/// <returns>16-bit CRC.</returns>
uint16_t CRC::addCRC16(uint8_t* in, uint32_t bitLength)
{
    assert(in != nullptr);

    uint16_t crc = createCRC16(in, bitLength);

    uint8_t temp[2U];
    temp[0U] = (crc >> 8) & 0xFFU;
    temp[1U] = (crc >> 0) & 0xFFU;

    uint32_t n = bitLength;
    for (uint32_t i = 0U; i < 16U; i++, n++) {
        bool b = READ_BIT(temp, i);
        WRITE_BIT(in, n, b);
    }

#if DEBUG_CRC_ADD
    LogDebug(LOG_HOST, "CRC::addCRC16(), crc = $%04X, bitlen = %u", crc, bitLength);
#endif
    return crc;
}

/// <summary>
/// Generate 9-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns></returns>
uint16_t CRC::createCRC9(const uint8_t* in, uint32_t bitLength)
{
    uint16_t crc = 0U;

    for (uint32_t i = 0U; i < bitLength; i++) {
        bool bit1 = READ_BIT(in, i) != 0x00U;
        bool bit2 = (crc & 0x100U) == 0x100U;

        crc <<= 1;

        if (bit1 ^ bit2)
            crc ^= 0x59U;
    }

    crc = ~crc;
    return crc & 0x1FFU;
}

/// <summary>
/// Generate 16-bit CRC.
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns></returns>
uint16_t CRC::createCRC16(const uint8_t* in, uint32_t bitLength)
{
    uint16_t crc = 0xFFFFU;

    for (uint32_t i = 0U; i < bitLength; i++) {
        bool bit1 = READ_BIT(in, i) != 0x00U;
        bool bit2 = (crc & 0x8000U) == 0x8000U;

        crc <<= 1;

        if (bit1 ^ bit2)
            crc ^= 0x1021U;
    }

    return crc & 0xFFFFU;
}

// ---------------------------------------------------------------------------
//  Private Static Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns></returns>
uint8_t CRC::createCRC6(const uint8_t* in, uint32_t bitLength)
{
    uint8_t crc = 0x3FU;

    for (uint32_t i = 0U; i < bitLength; i++) {
        bool bit1 = READ_BIT(in, i) != 0x00U;
        bool bit2 = (crc & 0x20U) == 0x20U;

        crc <<= 1;

        if (bit1 ^ bit2)
            crc ^= 0x27U;
    }

    return crc & 0x3FU;
}

/// <summary>
///
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns></returns>
uint16_t CRC::createCRC12(const uint8_t* in, uint32_t bitLength)
{
    uint16_t crc = 0x0FFFU;

    for (uint32_t i = 0U; i < bitLength; i++) {
        bool bit1 = READ_BIT(in, i) != 0x00U;
        bool bit2 = (crc & 0x0800U) == 0x0800U;

        crc <<= 1;

        if (bit1 ^ bit2)
            crc ^= 0x080FU;
    }

    return crc & 0x0FFFU;
}

/// <summary>
///
/// </summary>
/// <param name="in">Input byte array.</param>
/// <param name="bitLength">Length of byte array in bits.</param>
/// <returns></returns>
uint16_t CRC::createCRC15(const uint8_t* in, uint32_t bitLength)
{
    uint16_t crc = 0x7FFFU;

    for (uint32_t i = 0U; i < bitLength; i++) {
        bool bit1 = READ_BIT(in, i) != 0x00U;
        bool bit2 = (crc & 0x4000U) == 0x4000U;

        crc <<= 1;

        if (bit1 ^ bit2)
            crc ^= 0x4CC5U;
    }

    return crc & 0x7FFFU;
}
