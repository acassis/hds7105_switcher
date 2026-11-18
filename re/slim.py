#!/usr/bin/env python3
"""Filter an annotated dis.py dump down to semantic lines (calls,
compares, branches, constants and annotated strings).

usage: slim.py dump.txt 'funcregex'"""
import re, sys
if len(sys.argv) < 3:
    sys.exit(__doc__)
dump, pat = sys.argv[1], re.compile(sys.argv[2])
NOISE = re.compile(r'call\s+\S+ <(QString::~QString|QString::QString\(QString const&\)|QByteArray::~QByteArray|'
                   r'QFlags<|QHostAddress::~|_Unwind_Resume|QMetaObject::Connection::~Connection|QDebug::~QDebug|'
                   r'QMessageLogger::QMessageLogger|QMessageLogger::debug|QtPrivate::|QListData|operator delete|'
                   r'QList<QString>::~|QStringList::~|QFont::~|QIcon::~|QPixmap::~|QSize::QSize|QPalette::~|QLatin1String::QLatin1String)')
KEEP = re.compile(r'call|; |cmp|test|\bj[a-z]+\s|0x[0-9a-f]+$|0x[0-9a-f]+\]|ret')
on = False
for line in open(dump):
    if line.startswith('===='):
        on = bool(pat.search(line))
        if on:
            print('\n' + line.rstrip())
        continue
    if not on:
        continue
    if NOISE.search(line):
        continue
    if not KEEP.search(line):
        continue
    if re.search(r'(push|pop|leave|nop)\b', line):
        continue
    if re.search(r'mov    DWORD PTR \[ebp-0x[0-9a-f]+\],(ecx|eax|edx)$', line):
        continue
    if re.search(r'sub    esp,0x[0-9a-f]+$', line):
        continue
    line = re.sub(r'<([^>]{90})[^>]*>', r'<\1…>', line.rstrip())
    print(line)
