/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/MainWindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    const uint offsetsAndSize[66];
    char stringdata0[537];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 10), // "MainWindow"
QT_MOC_LITERAL(11, 15), // "browseVideoPath"
QT_MOC_LITERAL(27, 0), // ""
QT_MOC_LITERAL(28, 20), // "browseAnalysisOutDir"
QT_MOC_LITERAL(49, 20), // "browseRiskConfigPath"
QT_MOC_LITERAL(70, 16), // "browseInputImage"
QT_MOC_LITERAL(87, 17), // "browseOutputImage"
QT_MOC_LITERAL(105, 16), // "browsePythonPath"
QT_MOC_LITERAL(122, 16), // "browseScriptPath"
QT_MOC_LITERAL(139, 13), // "browseCfgPath"
QT_MOC_LITERAL(153, 16), // "browseWeightPath"
QT_MOC_LITERAL(170, 17), // "browseSummaryPath"
QT_MOC_LITERAL(188, 12), // "runInference"
QT_MOC_LITERAL(201, 16), // "runVideoAnalysis"
QT_MOC_LITERAL(218, 19), // "onSourceModeChanged"
QT_MOC_LITERAL(238, 5), // "index"
QT_MOC_LITERAL(244, 17), // "detectCameraIndex"
QT_MOC_LITERAL(262, 18), // "refreshLivePreview"
QT_MOC_LITERAL(281, 21), // "validateConfiguration"
QT_MOC_LITERAL(303, 13), // "saveLogToFile"
QT_MOC_LITERAL(317, 17), // "runBatchInference"
QT_MOC_LITERAL(335, 20), // "cancelBatchInference"
QT_MOC_LITERAL(356, 15), // "loadRiskSummary"
QT_MOC_LITERAL(372, 14), // "copyRiskAdvice"
QT_MOC_LITERAL(387, 20), // "onProcessStdoutReady"
QT_MOC_LITERAL(408, 20), // "onProcessStderrReady"
QT_MOC_LITERAL(429, 20), // "onSummaryPathChanged"
QT_MOC_LITERAL(450, 4), // "path"
QT_MOC_LITERAL(455, 20), // "onSummaryFileChanged"
QT_MOC_LITERAL(476, 19), // "onInferenceFinished"
QT_MOC_LITERAL(496, 8), // "exitCode"
QT_MOC_LITERAL(505, 20), // "QProcess::ExitStatus"
QT_MOC_LITERAL(526, 10) // "exitStatus"

    },
    "MainWindow\0browseVideoPath\0\0"
    "browseAnalysisOutDir\0browseRiskConfigPath\0"
    "browseInputImage\0browseOutputImage\0"
    "browsePythonPath\0browseScriptPath\0"
    "browseCfgPath\0browseWeightPath\0"
    "browseSummaryPath\0runInference\0"
    "runVideoAnalysis\0onSourceModeChanged\0"
    "index\0detectCameraIndex\0refreshLivePreview\0"
    "validateConfiguration\0saveLogToFile\0"
    "runBatchInference\0cancelBatchInference\0"
    "loadRiskSummary\0copyRiskAdvice\0"
    "onProcessStdoutReady\0onProcessStderrReady\0"
    "onSummaryPathChanged\0path\0"
    "onSummaryFileChanged\0onInferenceFinished\0"
    "exitCode\0QProcess::ExitStatus\0exitStatus"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      26,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  170,    2, 0x08,    1 /* Private */,
       3,    0,  171,    2, 0x08,    2 /* Private */,
       4,    0,  172,    2, 0x08,    3 /* Private */,
       5,    0,  173,    2, 0x08,    4 /* Private */,
       6,    0,  174,    2, 0x08,    5 /* Private */,
       7,    0,  175,    2, 0x08,    6 /* Private */,
       8,    0,  176,    2, 0x08,    7 /* Private */,
       9,    0,  177,    2, 0x08,    8 /* Private */,
      10,    0,  178,    2, 0x08,    9 /* Private */,
      11,    0,  179,    2, 0x08,   10 /* Private */,
      12,    0,  180,    2, 0x08,   11 /* Private */,
      13,    0,  181,    2, 0x08,   12 /* Private */,
      14,    1,  182,    2, 0x08,   13 /* Private */,
      16,    0,  185,    2, 0x08,   15 /* Private */,
      17,    0,  186,    2, 0x08,   16 /* Private */,
      18,    0,  187,    2, 0x08,   17 /* Private */,
      19,    0,  188,    2, 0x08,   18 /* Private */,
      20,    0,  189,    2, 0x08,   19 /* Private */,
      21,    0,  190,    2, 0x08,   20 /* Private */,
      22,    0,  191,    2, 0x08,   21 /* Private */,
      23,    0,  192,    2, 0x08,   22 /* Private */,
      24,    0,  193,    2, 0x08,   23 /* Private */,
      25,    0,  194,    2, 0x08,   24 /* Private */,
      26,    1,  195,    2, 0x08,   25 /* Private */,
      28,    1,  198,    2, 0x08,   27 /* Private */,
      29,    2,  201,    2, 0x08,   29 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   27,
    QMetaType::Void, QMetaType::QString,   27,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 31,   30,   32,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->browseVideoPath(); break;
        case 1: _t->browseAnalysisOutDir(); break;
        case 2: _t->browseRiskConfigPath(); break;
        case 3: _t->browseInputImage(); break;
        case 4: _t->browseOutputImage(); break;
        case 5: _t->browsePythonPath(); break;
        case 6: _t->browseScriptPath(); break;
        case 7: _t->browseCfgPath(); break;
        case 8: _t->browseWeightPath(); break;
        case 9: _t->browseSummaryPath(); break;
        case 10: _t->runInference(); break;
        case 11: _t->runVideoAnalysis(); break;
        case 12: _t->onSourceModeChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->detectCameraIndex(); break;
        case 14: _t->refreshLivePreview(); break;
        case 15: _t->validateConfiguration(); break;
        case 16: _t->saveLogToFile(); break;
        case 17: _t->runBatchInference(); break;
        case 18: _t->cancelBatchInference(); break;
        case 19: _t->loadRiskSummary(); break;
        case 20: _t->copyRiskAdvice(); break;
        case 21: _t->onProcessStdoutReady(); break;
        case 22: _t->onProcessStderrReady(); break;
        case 23: _t->onSummaryPathChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 24: _t->onSummaryFileChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->onInferenceFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.offsetsAndSize,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_MainWindow_t
, QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>
, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<QProcess::ExitStatus, std::false_type>


>,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 26)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 26;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 26)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 26;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
