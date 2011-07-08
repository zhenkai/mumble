/****************************************************************************
** Meta object code from reading C++ file 'sessionenum.h'
**
** Created: Thu Jul 7 16:58:54 2011
**      by: The Qt Meta Object Compiler version 62 (Qt 4.7.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "sessionenum.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'sessionenum.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.7.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_SessionEnum[] = {

 // content:
       5,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: signature, parameters, type, tag, flags
      15,   13,   12,   12, 0x05,
      40,   12,   12,   12, 0x05,

 // slots: signature, parameters, type, tag, flags
      59,   12,   12,   12, 0x08,
      71,   12,   12,   12, 0x08,
      90,   12,   12,   12, 0x08,
     109,   12,   12,   12, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_SessionEnum[] = {
    "SessionEnum\0\0,\0expired(QString,QString)\0"
    "add(Announcement*)\0enumerate()\0"
    "enumeratePriConf()\0enumeratePubConf()\0"
    "checkAlive()\0"
};

const QMetaObject SessionEnum::staticMetaObject = {
    { &QThread::staticMetaObject, qt_meta_stringdata_SessionEnum,
      qt_meta_data_SessionEnum, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &SessionEnum::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *SessionEnum::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *SessionEnum::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_SessionEnum))
        return static_cast<void*>(const_cast< SessionEnum*>(this));
    return QThread::qt_metacast(_clname);
}

int SessionEnum::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: expired((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< QString(*)>(_a[2]))); break;
        case 1: add((*reinterpret_cast< Announcement*(*)>(_a[1]))); break;
        case 2: enumerate(); break;
        case 3: enumeratePriConf(); break;
        case 4: enumeratePubConf(); break;
        case 5: checkAlive(); break;
        default: ;
        }
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void SessionEnum::expired(QString _t1, QString _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void SessionEnum::add(Announcement * _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_END_MOC_NAMESPACE
