#include <klocale.h>
/****************************************************************************
** Form implementation generated from reading ui file '../../../../kame/graph/forms/graphnurlform.ui'
**
** Created: 水  2 1 03:43:16 2006
**      by: The User Interface Compiler ($Id: graphnurlform.cpp,v 1.1 2006/02/01 18:44:04 northriv Exp $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "graphnurlform.h"

#include <qvariant.h>
#include <kurlrequester.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qaction.h>
#include <qmenubar.h>
#include <qpopupmenu.h>
#include <qtoolbar.h>
#include <qimage.h>
#include <qpixmap.h>

#include "../../graph/graphwidget.h"
static const unsigned char img0_graphnurlform[] = { 
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x30,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x57, 0x02, 0xf9, 0x87, 0x00, 0x00, 0x07,
    0x29, 0x49, 0x44, 0x41, 0x54, 0x68, 0x81, 0xed, 0x99, 0x7f, 0x4c, 0x94,
    0xf7, 0x1d, 0xc7, 0xdf, 0xdf, 0xe7, 0x7e, 0x70, 0xde, 0xee, 0x04, 0xd6,
    0x43, 0xe5, 0x60, 0x55, 0x14, 0x86, 0xa2, 0x2e, 0x6a, 0xe8, 0x42, 0x02,
    0xb5, 0x96, 0xcc, 0x4a, 0xfd, 0xc1, 0xe2, 0x74, 0x6e, 0xff, 0xac, 0x59,
    0xff, 0xa0, 0xb0, 0xaa, 0x9b, 0x0d, 0x6c, 0x54, 0xd6, 0xda, 0xa5, 0x60,
    0x46, 0xd3, 0x2d, 0x4a, 0x5a, 0x9d, 0x8c, 0xda, 0x9a, 0xa2, 0xf3, 0x47,
    0x53, 0xe9, 0xb4, 0xa5, 0x29, 0x20, 0x65, 0x9a, 0xa8, 0xa3, 0x25, 0x73,
    0x13, 0x36, 0xda, 0x94, 0x0d, 0xb3, 0x6a, 0x85, 0x3b, 0x82, 0x22, 0x1c,
    0x77, 0xf7, 0xdc, 0xf3, 0x7d, 0xef, 0x8f, 0x47, 0xa0, 0xf4, 0x38, 0x38,
    0x50, 0x64, 0xa4, 0xbe, 0x93, 0x27, 0xb9, 0xe4, 0xfb, 0x79, 0x9e, 0xef,
    0xfb, 0xf5, 0xf9, 0xfe, 0xfe, 0x9e, 0x20, 0x89, 0xe9, 0x2c, 0x65, 0xaa,
    0x0d, 0xdc, 0xa9, 0xee, 0x03, 0x4c, 0xb5, 0xee, 0x03, 0x4c, 0xb5, 0xa6,
    0x3d, 0x80, 0x31, 0x54, 0x01, 0x49, 0xfa, 0x7c, 0x3e, 0xa8, 0xaa, 0x7a,
    0x2f, 0xfd, 0x04, 0x49, 0x51, 0x14, 0x58, 0x2c, 0x16, 0x18, 0x0c, 0x06,
    0x31, 0x52, 0x79, 0x48, 0x80, 0x9b, 0x37, 0x6f, 0xa2, 0xa5, 0xa5, 0x05,
    0x2e, 0x97, 0x6b, 0xf2, 0xdc, 0x85, 0x21, 0x8b, 0xc5, 0x82, 0x94, 0x94,
    0x14, 0xc4, 0xc7, 0xc7, 0x53, 0x51, 0x94, 0x20, 0x88, 0x11, 0x01, 0xa4,
    0x94, 0x74, 0xbb, 0xdd, 0xa8, 0xab, 0xab, 0xc3, 0xe9, 0xd3, 0xa7, 0xa1,
    0x69, 0xda, 0xe4, 0x3b, 0x0d, 0xa1, 0xe4, 0xe4, 0x64, 0xe4, 0xe6, 0xe6,
    0x62, 0xd6, 0xac, 0x59, 0xb0, 0x58, 0x2c, 0x41, 0xe5, 0x21, 0x5b, 0x40,
    0xd3, 0x34, 0xf4, 0xf6, 0xf6, 0xc2, 0xed, 0x76, 0x23, 0x2d, 0x2d, 0x0d,
    0x06, 0x83, 0x61, 0x52, 0x8d, 0x8e, 0xa4, 0xb6, 0xb6, 0x36, 0x74, 0x74,
    0x74, 0x20, 0x10, 0x08, 0x20, 0xd4, 0x8e, 0x21, 0x24, 0xc0, 0x80, 0x0c,
    0x06, 0x03, 0x32, 0x33, 0x33, 0xb1, 0x6c, 0xd9, 0x32, 0x08, 0x31, 0x62,
    0x37, 0x9c, 0x14, 0x75, 0x75, 0x75, 0xa1, 0xac, 0xac, 0x0c, 0x5e, 0xaf,
    0x77, 0xd4, 0xb8, 0x31, 0x01, 0x00, 0x7d, 0x20, 0x25, 0x24, 0x24, 0x20,
    0x3a, 0x3a, 0xfa, 0xae, 0x98, 0x1b, 0x4b, 0xaa, 0xaa, 0xa2, 0xb1, 0xb1,
    0x31, 0xac, 0xd8, 0xb0, 0x00, 0x06, 0x83, 0x8d, 0xe3, 0x0a, 0x9f, 0xb0,
    0xc6, 0x33, 0xe6, 0xa6, 0xfd, 0x3a, 0xf0, 0xf5, 0x06, 0x68, 0xfc, 0x44,
    0x43, 0x45, 0x8d, 0x1f, 0x7e, 0x39, 0x75, 0x67, 0x8a, 0x09, 0x75, 0x6a,
    0x02, 0x78, 0xb5, 0xca, 0x8f, 0x82, 0xed, 0x46, 0xf8, 0x7b, 0xcc, 0x38,
    0xf6, 0x03, 0x15, 0xc7, 0xff, 0xa0, 0xc0, 0x31, 0xe3, 0xf6, 0x54, 0xdb,
    0xdc, 0x0c, 0x9c, 0x39, 0x03, 0x76, 0x77, 0x43, 0x24, 0x25, 0x01, 0xeb,
    0xd6, 0x01, 0x51, 0x51, 0x77, 0xd1, 0xf6, 0x90, 0xc6, 0x0d, 0xd0, 0xeb,
    0x23, 0x9e, 0x2e, 0x51, 0x51, 0xf9, 0x8a, 0x19, 0x82, 0x80, 0x35, 0x4a,
    0xa2, 0xfe, 0xa4, 0x09, 0x0f, 0x7d, 0xa2, 0xa1, 0xfa, 0xc5, 0x8f, 0xb1,
    0xb0, 0xb4, 0x10, 0xa8, 0xaf, 0x1f, 0x8c, 0x27, 0x00, 0xd8, 0x6c, 0x10,
    0xcf, 0x3f, 0x0f, 0xe4, 0xe7, 0x03, 0x77, 0x79, 0x3d, 0x19, 0x57, 0x17,
    0x6a, 0x73, 0x6b, 0x58, 0xb9, 0x25, 0x80, 0xca, 0x32, 0x33, 0x2c, 0xd1,
    0x1a, 0xf6, 0xbd, 0xa9, 0xa2, 0xa9, 0x49, 0x62, 0x49, 0x7a, 0x00, 0x8f,
    0xb8, 0x0f, 0xe2, 0xc1, 0xf5, 0x0f, 0xeb, 0xe6, 0xad, 0x56, 0x60, 0xe3,
    0x46, 0x60, 0xfb, 0x76, 0x20, 0x35, 0x15, 0xe8, 0xed, 0x05, 0x0b, 0x0b,
    0xc1, 0xcd, 0x9b, 0x81, 0xbe, 0xbe, 0x3b, 0x32, 0xfc, 0xc1, 0x25, 0x95,
    0x27, 0xce, 0xa9, 0x83, 0x7d, 0x36, 0xec, 0x16, 0xa8, 0xf9, 0x87, 0xc4,
    0x8e, 0x67, 0x00, 0x77, 0xbb, 0x09, 0x71, 0x29, 0x01, 0x1c, 0x7f, 0x03,
    0x48, 0x5f, 0x64, 0x02, 0x00, 0x5c, 0x7c, 0xf4, 0x65, 0x58, 0x6a, 0x0a,
    0xa1, 0x48, 0xe2, 0xe4, 0xec, 0x6c, 0x5c, 0xda, 0xb1, 0x07, 0x2f, 0x3c,
    0x9b, 0x00, 0x23, 0x04, 0x20, 0x25, 0x70, 0xf4, 0x28, 0x98, 0x97, 0x07,
    0xbc, 0xf3, 0x0e, 0xb8, 0x65, 0x0b, 0x44, 0x55, 0x15, 0x60, 0x36, 0x8f,
    0xcb, 0x78, 0x8f, 0x26, 0xb1, 0xad, 0xd8, 0xcf, 0x43, 0x7b, 0x4d, 0x30,
    0x5a, 0x89, 0xcc, 0xff, 0xaa, 0x74, 0xc0, 0x24, 0x40, 0x32, 0xe8, 0xd1,
    0x34, 0x8d, 0xad, 0xad, 0xad, 0x2c, 0x28, 0x28, 0x60, 0x42, 0xe2, 0x02,
    0x66, 0x3d, 0x55, 0x4f, 0xc3, 0x37, 0x25, 0x61, 0x27, 0x1f, 0xf9, 0x89,
    0x9f, 0xd7, 0x6e, 0x6a, 0x1c, 0xd4, 0xfe, 0xfd, 0x94, 0x42, 0x50, 0x2a,
    0x0a, 0x2f, 0x3c, 0xb5, 0x93, 0xa6, 0x98, 0x00, 0x61, 0x27, 0xd7, 0xe4,
    0xf9, 0xe8, 0xf2, 0x06, 0x86, 0xe2, 0x2e, 0x5c, 0xa0, 0x8c, 0x8c, 0xa4,
    0x04, 0x28, 0x9f, 0x7c, 0x92, 0x94, 0x92, 0xa1, 0xe4, 0xf5, 0x7a, 0xd9,
    0xd0, 0xd0, 0xc0, 0xac, 0xac, 0x2c, 0xae, 0x5a, 0xb5, 0x8a, 0xa5, 0x15,
    0x67, 0xf9, 0xe0, 0x43, 0x7e, 0xc2, 0x4e, 0x9a, 0xe2, 0x34, 0xee, 0x3e,
    0xe2, 0xe5, 0x80, 0xd7, 0x51, 0x01, 0x7e, 0xf1, 0xec, 0x0b, 0x8c, 0x58,
    0x58, 0x45, 0xd8, 0x49, 0x25, 0x5a, 0xf2, 0xe7, 0x65, 0x5e, 0xfa, 0xf8,
    0xa5, 0x8a, 0x4f, 0x9d, 0xa2, 0x34, 0x99, 0x28, 0x01, 0xb2, 0xa4, 0x84,
    0x94, 0x92, 0x55, 0x17, 0xfd, 0x8c, 0x4c, 0xd0, 0x61, 0x13, 0x1f, 0x56,
    0xf9, 0xf7, 0xab, 0xea, 0x50, 0x7c, 0x6d, 0x2d, 0x65, 0x44, 0x84, 0x1e,
    0xbf, 0x67, 0xcf, 0x98, 0x00, 0x6b, 0xb2, 0xb3, 0xe9, 0xf8, 0x4e, 0x39,
    0x0d, 0x0f, 0xe8, 0x49, 0x49, 0x5e, 0x19, 0xe0, 0xf9, 0x7f, 0xfb, 0x07,
    0xcd, 0x8f, 0x0a, 0x50, 0x7d, 0xae, 0x9d, 0x73, 0x56, 0xb8, 0x08, 0x3b,
    0x69, 0x89, 0x0d, 0xf0, 0xc0, 0x7b, 0xdd, 0x1c, 0x96, 0xb3, 0xe6, 0x66,
    0xca, 0x99, 0x33, 0xf5, 0x8c, 0x6e, 0xdd, 0x3a, 0x2c, 0xa3, 0xcd, 0x5f,
    0xa8, 0x4c, 0x5e, 0xa5, 0x12, 0x76, 0x72, 0xe6, 0x7c, 0x8d, 0x6f, 0x9d,
    0xf3, 0x07, 0xb7, 0x98, 0xd9, 0x4c, 0x36, 0x34, 0x84, 0x04, 0xa8, 0xac,
    0xfe, 0x88, 0x8e, 0x94, 0x2b, 0x84, 0x9d, 0x44, 0xb4, 0x64, 0x5e, 0x69,
    0x3f, 0x7b, 0xfc, 0xda, 0x30, 0xf3, 0x21, 0x01, 0x8e, 0x35, 0xf8, 0x69,
    0xfb, 0xb6, 0x6e, 0xc0, 0x32, 0xef, 0x0a, 0x4b, 0xca, 0x4f, 0xb2, 0xb3,
    0xb3, 0x73, 0xa8, 0x86, 0xae, 0x2e, 0xca, 0xa4, 0x24, 0xdd, 0xfc, 0xe3,
    0x8f, 0x93, 0xaa, 0xfa, 0x55, 0x0f, 0xec, 0xf6, 0x69, 0xdc, 0xb0, 0x55,
    0x6f, 0x76, 0xe5, 0x01, 0xc9, 0xa2, 0x03, 0x3e, 0x06, 0x6e, 0xa7, 0x40,
    0xe6, 0xe4, 0xe8, 0xef, 0xc6, 0xc5, 0x91, 0x9f, 0x7f, 0x3e, 0xec, 0x3d,
    0x55, 0x4a, 0xbe, 0x74, 0xa4, 0x9f, 0x11, 0x4e, 0x3d, 0xeb, 0xb6, 0x44,
    0x37, 0x5f, 0x3a, 0xf4, 0x37, 0x7a, 0x3c, 0x9e, 0x20, 0xf3, 0x41, 0x00,
    0x7e, 0x4d, 0xf2, 0x57, 0xfb, 0xbd, 0x54, 0x62, 0xf4, 0x2e, 0x10, 0x9b,
    0xda, 0xc8, 0x79, 0x8b, 0x96, 0xb3, 0xa2, 0xa2, 0x62, 0x08, 0x40, 0x55,
    0x29, 0xd7, 0xaf, 0xd7, 0x0d, 0x24, 0x27, 0x93, 0x5d, 0x5d, 0x23, 0x66,
    0x91, 0x24, 0x03, 0x94, 0x2c, 0xfa, 0xa3, 0x8f, 0x4a, 0x34, 0x09, 0x3b,
    0xb9, 0x61, 0x9b, 0x8f, 0x5d, 0x1e, 0x8d, 0xf4, 0x78, 0x28, 0x57, 0xac,
    0xd0, 0xbf, 0x91, 0x91, 0x41, 0x7a, 0xbd, 0x24, 0xc9, 0xff, 0xb8, 0x02,
    0x7c, 0x34, 0xc7, 0xab, 0x67, 0xdd, 0x4e, 0x46, 0x2d, 0x7d, 0x97, 0x2b,
    0xd7, 0x6e, 0x64, 0x4d, 0x4d, 0xcd, 0xd8, 0x00, 0x9d, 0x9e, 0x00, 0xbf,
    0xf7, 0x53, 0x3d, 0x63, 0x22, 0x46, 0x63, 0x4e, 0xf1, 0x15, 0x3e, 0x93,
    0xff, 0x4b, 0xce, 0x9f, 0x3f, 0x7f, 0x18, 0x80, 0xdc, 0xb9, 0x53, 0xaf,
    0xd8, 0x66, 0x23, 0x2f, 0x5f, 0x0e, 0x69, 0xfe, 0xcb, 0x3a, 0x7e, 0xc1,
    0x47, 0x5b, 0xa2, 0x9e, 0xd1, 0x45, 0x99, 0x2a, 0xff, 0x79, 0x5d, 0x25,
    0xdb, 0xda, 0x28, 0x1d, 0x0e, 0xfd, 0x5b, 0x39, 0x39, 0x7c, 0xf3, 0x6c,
    0x3f, 0xa3, 0x93, 0x34, 0xc2, 0x4e, 0x46, 0x2e, 0x54, 0x59, 0xb0, 0xf7,
    0x63, 0xae, 0xb9, 0x3d, 0x88, 0x47, 0x03, 0x50, 0x00, 0xc0, 0x03, 0xc9,
    0x8c, 0x8d, 0x1a, 0xea, 0xde, 0x36, 0x21, 0x3a, 0x4e, 0xc3, 0xdb, 0xa7,
    0x54, 0xe4, 0xff, 0xb0, 0x1f, 0x06, 0xf1, 0x95, 0x2d, 0x42, 0x65, 0x25,
    0x50, 0x5a, 0x0a, 0x18, 0x0c, 0x10, 0x87, 0x0e, 0x01, 0x4b, 0x96, 0x84,
    0x35, 0x05, 0x6e, 0x49, 0x33, 0xe3, 0xfc, 0x5f, 0x88, 0x05, 0x4b, 0x35,
    0xfc, 0xeb, 0x23, 0x23, 0xd2, 0x56, 0x0a, 0xfc, 0xd9, 0xfd, 0x2d, 0x88,
    0x63, 0xc7, 0x80, 0x88, 0x08, 0xa0, 0xa2, 0x02, 0x9f, 0xfe, 0xe8, 0xb7,
    0xb8, 0x71, 0x5d, 0x20, 0x73, 0x93, 0x8a, 0xbf, 0xd6, 0xf9, 0xb1, 0x7e,
    0x59, 0x2f, 0xc2, 0x39, 0x7d, 0x28, 0x00, 0x40, 0x10, 0x52, 0x21, 0x56,
    0xac, 0x0a, 0xe0, 0xfc, 0x59, 0xe2, 0xfb, 0xdf, 0x35, 0x05, 0x07, 0x7e,
    0xf8, 0x21, 0x98, 0x9b, 0x0b, 0x90, 0xc0, 0xae, 0x5d, 0xc0, 0xa6, 0x4d,
    0x61, 0x99, 0x1f, 0xd0, 0x52, 0xa7, 0x11, 0x17, 0x6b, 0x05, 0xd6, 0xfc,
    0x58, 0x45, 0xcf, 0x17, 0x06, 0x6c, 0xce, 0x36, 0x20, 0xaf, 0x29, 0x03,
    0x85, 0x0b, 0xf6, 0x80, 0x42, 0xe0, 0x37, 0x1d, 0xc5, 0x78, 0x2f, 0x7b,
    0x37, 0x6a, 0xde, 0x30, 0x62, 0x9e, 0x23, 0xfc, 0xd5, 0xda, 0x08, 0x00,
    0xdf, 0x80, 0x41, 0x34, 0x57, 0x0b, 0x1a, 0x20, 0x60, 0x84, 0x10, 0x52,
    0x0e, 0xdf, 0x9d, 0x45, 0xb6, 0xb4, 0x20, 0xea, 0xe0, 0x41, 0xa0, 0xbf,
    0x1f, 0x78, 0xe2, 0x09, 0x88, 0xe7, 0x9e, 0x1b, 0x97, 0xf9, 0x01, 0x39,
    0xac, 0x0a, 0x4e, 0x1f, 0x10, 0x28, 0x5a, 0xea, 0xc7, 0xef, 0x7e, 0x6d,
    0x46, 0x79, 0x49, 0x04, 0x80, 0x9f, 0x61, 0xee, 0xea, 0x0e, 0x3c, 0x5d,
    0xfb, 0x22, 0xb2, 0xfe, 0xb4, 0x0b, 0x70, 0xb8, 0x81, 0xe2, 0xe2, 0xb0,
    0xbf, 0x39, 0xb8, 0x95, 0x88, 0x80, 0x22, 0x8c, 0x08, 0x3e, 0x33, 0xa6,
    0xf5, 0xf4, 0x60, 0xc3, 0xbe, 0x7d, 0x50, 0x6e, 0xdd, 0x02, 0xd6, 0xad,
    0x83, 0xa8, 0xa8, 0x00, 0x94, 0x89, 0x6f, 0x62, 0x4d, 0x8a, 0xc0, 0xcb,
    0xdb, 0xcd, 0x38, 0xf2, 0xae, 0x1f, 0x09, 0x19, 0x3e, 0x14, 0xee, 0x56,
    0x91, 0xf3, 0xc1, 0x2e, 0x88, 0xbd, 0x7b, 0xf5, 0x7d, 0x52, 0x59, 0x19,
    0x8c, 0x19, 0x19, 0xb0, 0x35, 0x35, 0x41, 0x84, 0x73, 0x73, 0x3e, 0xd2,
    0xc0, 0xd0, 0x34, 0x8d, 0x9f, 0x5e, 0xbe, 0xcc, 0xfa, 0xd5, 0xab, 0xe9,
    0x13, 0x82, 0x12, 0xa0, 0xba, 0x76, 0x2d, 0xd9, 0xd7, 0x17, 0xd6, 0xa0,
    0x9d, 0xb0, 0xaa, 0xab, 0x29, 0xe7, 0xcc, 0xd1, 0x07, 0x36, 0xc0, 0x16,
    0xab, 0x95, 0x47, 0x13, 0x12, 0xd8, 0x92, 0x9f, 0x4f, 0x5f, 0x79, 0x39,
    0xd9, 0xda, 0x1a, 0x34, 0x90, 0x83, 0x01, 0x5c, 0x2e, 0xca, 0xd7, 0x5f,
    0xa7, 0x7f, 0x60, 0x9e, 0x07, 0xd8, 0x96, 0x95, 0x45, 0xd7, 0xd5, 0xab,
    0x93, 0x6b, 0x7e, 0x40, 0x9d, 0x9d, 0x0c, 0x6c, 0xdb, 0x46, 0xcd, 0x62,
    0x19, 0xac, 0x7f, 0xf0, 0x49, 0x4d, 0x0d, 0x0d, 0x20, 0xf3, 0xf2, 0xf4,
    0x69, 0xed, 0x76, 0xc6, 0x25, 0x40, 0x97, 0xc5, 0xc2, 0x1d, 0x4e, 0xe7,
    0xf0, 0x75, 0xe0, 0x1e, 0xc8, 0xeb, 0xf5, 0xf2, 0x7c, 0x75, 0x35, 0x7f,
    0xbf, 0x7c, 0x39, 0xdf, 0x8a, 0x8d, 0xe5, 0xf5, 0xf4, 0x74, 0x06, 0xb2,
    0xb3, 0xc9, 0x13, 0x27, 0x82, 0x00, 0x86, 0x76, 0xa3, 0x97, 0x2e, 0x01,
    0x6e, 0xb7, 0x3e, 0xad, 0x2d, 0x5e, 0x8c, 0xeb, 0x8f, 0x3d, 0x86, 0x57,
    0x6e, 0xdd, 0xc2, 0xa9, 0xf7, 0xdf, 0xc7, 0xe2, 0x09, 0xf7, 0xf8, 0x89,
    0xcb, 0x6f, 0xb5, 0xa2, 0x76, 0xf6, 0x6c, 0x78, 0x23, 0x23, 0x11, 0x59,
    0x54, 0x84, 0x8c, 0x8c, 0x0c, 0xcc, 0x98, 0x31, 0x23, 0xf4, 0xcd, 0x9c,
    0x38, 0x73, 0x06, 0xb8, 0x76, 0x0d, 0x88, 0x89, 0x81, 0xb4, 0xd9, 0xd0,
    0xf3, 0xd9, 0x67, 0x50, 0x5f, 0x7b, 0xed, 0xde, 0xba, 0x9e, 0x80, 0x86,
    0x5a, 0xc0, 0x6a, 0x15, 0x48, 0x4c, 0xd4, 0x7f, 0xcb, 0xe0, 0x43, 0xae,
    0xa6, 0x69, 0xe8, 0xbb, 0xc3, 0xc3, 0x48, 0xb8, 0x1a, 0xcf, 0xb5, 0x4a,
    0x58, 0x07, 0x1a, 0xaf, 0xd7, 0x8b, 0xa6, 0xa6, 0xa6, 0x09, 0x1b, 0x9a,
    0x88, 0x3c, 0x1e, 0x4f, 0x58, 0x71, 0x63, 0x02, 0x78, 0x3c, 0x1e, 0x1c,
    0x3e, 0x7c, 0xf8, 0x8e, 0x0d, 0x4d, 0x44, 0xdd, 0xdd, 0xdd, 0x70, 0x3a,
    0x9d, 0xa3, 0xc6, 0x8c, 0x08, 0x20, 0x84, 0x80, 0xd9, 0x6c, 0x46, 0x6c,
    0x6c, 0x2c, 0xd2, 0xd3, 0xd3, 0x21, 0xa5, 0x9c, 0x14, 0x83, 0x63, 0xc9,
    0xe9, 0x74, 0x22, 0x3e, 0x3e, 0x1e, 0x56, 0xab, 0x15, 0x4a, 0x88, 0xc5,
    0x53, 0x30, 0xc4, 0x6a, 0xe7, 0xf1, 0x78, 0xd8, 0xde, 0xde, 0x8e, 0x1b,
    0x37, 0x6e, 0x4c, 0xa6, 0xc7, 0x31, 0x65, 0x36, 0x9b, 0x31, 0x77, 0xee,
    0x5c, 0x38, 0x1c, 0x0e, 0x88, 0x11, 0x6e, 0x97, 0x43, 0x02, 0x00, 0x80,
    0xa6, 0x69, 0x9c, 0xca, 0xff, 0x06, 0x00, 0xbd, 0x37, 0x18, 0x8d, 0xc6,
    0x11, 0xcd, 0x03, 0x63, 0x00, 0x4c, 0x07, 0x7d, 0xbd, 0xef, 0x46, 0xff,
    0x1f, 0x74, 0x1f, 0x60, 0xaa, 0x35, 0xed, 0x01, 0xfe, 0x07, 0x5e, 0xa3,
    0xd7, 0x68, 0xad, 0xcf, 0x76, 0xe5, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};


/*
 *  Constructs a FrmGraphNURL as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 */
FrmGraphNURL::FrmGraphNURL( QWidget* parent, const char* name, WFlags fl )
    : QMainWindow( parent, name, fl )
{
    (void)statusBar();
    QImage img;
    img.loadFromData( img0_graphnurlform, sizeof( img0_graphnurlform ), "PNG" );
    image0 = img;
    if ( !name )
	setName( "FrmGraphNURL" );
    setIcon( image0 );
    setCentralWidget( new QWidget( this, "qt_central_widget" ) );
    FrmGraphNURLLayout = new QGridLayout( centralWidget(), 1, 1, 2, 6, "FrmGraphNURLLayout"); 

    layout1 = new QHBoxLayout( 0, 0, 6, "layout1"); 

    m_url = new KURLRequester( centralWidget(), "m_url" );
    m_url->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)5, (QSizePolicy::SizeType)1, 0, 0, m_url->sizePolicy().hasHeightForWidth() ) );
    layout1->addWidget( m_url );

    m_btnDump = new QPushButton( centralWidget(), "m_btnDump" );
    layout1->addWidget( m_btnDump );

    FrmGraphNURLLayout->addLayout( layout1, 1, 0 );

    m_graphwidget = new XQGraph( centralWidget(), "m_graphwidget" );
    m_graphwidget->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)5, (QSizePolicy::SizeType)7, 0, 0, m_graphwidget->sizePolicy().hasHeightForWidth() ) );

    FrmGraphNURLLayout->addWidget( m_graphwidget, 0, 0 );

    // toolbars

    languageChange();
    resize( QSize(453, 324).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
FrmGraphNURL::~FrmGraphNURL()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void FrmGraphNURL::languageChange()
{
    setCaption( tr2i18n( "Graph" ) );
    m_btnDump->setText( tr2i18n( "DUMP" ) );
}

#include "graphnurlform.moc"
