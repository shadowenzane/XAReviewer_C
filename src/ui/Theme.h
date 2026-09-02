#pragma once

#include <QString>

// 深色医疗主题（对应 Python 医疗暗色 UI 风格）
class Theme
{
public:
    // 全局样式表（QSS）
    static QString styleSheet();

    // 调色板常量
    static const char* bgColor()        { return "#1e1f24"; } // 窗口底
    static const char* panelColor()     { return "#26272e"; } // 面板
    static const char* panelAltColor()  { return "#2e3038"; } // 面板悬浮/输入框
    static const char* borderColor()    { return "#3a3d47"; }
    static const char* textColor()      { return "#e0e0e0"; }
    static const char* textDimColor()   { return "#9a9aa5"; }
    static const char* accentColor()    { return "#3d8bff"; } // 主色（蓝）
    static const char* accentHoverColor(){ return "#5a9dff"; }
    static const char* dangerColor()    { return "#e05555"; }
    static const char* measureColor()   { return "#00e5ff"; } // 测量线（青）
};
