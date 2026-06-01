#pragma once

// =====================================================================
//  IconsFontAwesome6.h -- minimal Font Awesome 6 Free (Solid) glyph defs
// =====================================================================
//
//  Codepoints live in the Unicode Private Use Area; the strings below are
//  their UTF-8 encodings, so an icon can be concatenated inline with
//  normal text: ImGui::Button(ICON_FA_PLAY " Play").
//
//  fa-solid-900.ttf is merged into the ImGui font atlas over the range
//  [ICON_MIN_FA, ICON_MAX_FA] (see ImGuiLayer::onAttach). Full upstream
//  reference: https://github.com/juliettef/IconFontCppHeaders
//
// =====================================================================

#define FONT_ICON_FILE_NAME_FAS "fa-solid-900.ttf"

#define ICON_MIN_FA 0xe005
#define ICON_MAX_FA 0xf8ff

#define ICON_FA_PLAY             "\xef\x81\x8b"   // U+F04B  play
#define ICON_FA_STOP             "\xef\x81\x8d"   // U+F04D  stop
#define ICON_FA_XMARK            "\xef\x80\x8d"   // U+F00D  close
#define ICON_FA_MINUS            "\xef\x81\xa8"   // U+F068  minimize
#define ICON_FA_WINDOW_MAXIMIZE  "\xef\x8b\x90"   // U+F2D0  maximize
#define ICON_FA_WINDOW_RESTORE   "\xef\x8b\x92"   // U+F2D2  restore
#define ICON_FA_FLOPPY_DISK      "\xef\x83\x87"   // U+F0C7  save
#define ICON_FA_FOLDER_OPEN      "\xef\x81\xbc"   // U+F07C  open
#define ICON_FA_CAMERA           "\xef\x80\xb0"   // U+F030  screenshot
#define ICON_FA_CUBE             "\xef\x86\xb2"   // U+F1B2  entity / object
#define ICON_FA_LIST             "\xef\x80\xba"   // U+F03A  hierarchy
#define ICON_FA_CIRCLE_INFO      "\xef\x81\x9a"   // U+F05A  inspector / info
#define ICON_FA_TERMINAL         "\xef\x84\xa0"   // U+F120  console