// xctdprep.h - make imported XBLA data runnable by the recomp.
//
// Some XBLA titles (Bejeweled Blitz Live) ship content files XCTD-encoded
// (XCompress LZX, magic 0x0FF512ED). The Xbox 360 kernel decodes them
// transparently on read; the ReXGlue runtime does not. The import step
// therefore decodes them offline, in place, with the bundled `xctd` tool
// (project/tools/xctd), and adds the lowercase default.xex the runtime and
// game detection look for when the package spells it Default.xex.
//
// All functions are blocking and meant for an import worker thread.
#pragma once

#include <QString>
#include <QStringList>
#include <functional>

using PrepStatus = std::function<void(const QString &)>;

// Copy a directory tree (files and subfolders) from src into dst, which is
// created. Fails if dst already holds a file of the same name.
bool copyTree(const QString &src, const QString &dst, const PrepStatus &status,
              QString *error);

// XCTD-encoded files under dataDir (recursive); empty when the folder is
// already prepared.
QStringList findXctdFiles(const QString &dataDir);

// Decode every XCTD file under dataDir in place and ensure default.xex
// exists. Files that are not XCTD are left untouched, so running it on an
// already-prepared folder is a no-op.
bool prepareXctdData(const QString &dataDir, const PrepStatus &status,
                     QString *error);
