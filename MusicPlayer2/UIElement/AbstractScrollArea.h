#pragma once
#include "UIElement/UIElement.h"
#include <atomic>
#include <chrono>
namespace UiElement
{
    //滚动区域
    class AbstractScrollArea : public Element
    {
    public:
        virtual void Draw() override;
        virtual void DrawScrollArea() = 0;
        CRect GetScrollAreaRect() const;

        virtual bool LButtonUp(CPoint point) override;
        virtual bool LButtonDown(CPoint point) override;
        virtual bool MouseMove(CPoint point) override;
        virtual bool MouseWheel(int delta, CPoint point) override;
        virtual bool MouseLeave() override;

        virtual int GetScrollAreaHeight() = 0;
        void RestrictOffset();             //将滚动区域偏移量限制在正确的范围
        void ScrollBy(int distance);       //滚动指定的距离，界面启用了平滑滚动时以动画的方式滚动
        static bool IsSmoothScrolling();   //是否有滚动区域正在进行平滑滚动动画（用于界面线程临时提高刷新率）

    protected:
        CRect m_scroll_area_rect;       //滚动区域的矩形区域
        CRect m_client_area_rect;       //获取客户区域的矩形（不包含滚动条，不包含控件外区域）

        bool mouse_pressed{ };          //鼠标左键是否按下
        bool hover{};                   //指标指向播放列表区域
        CPoint mouse_pos;               //鼠标指向的区域
        CPoint mouse_pressed_pos;       //鼠标按下时的位置
        int mouse_pressed_offset{};     //鼠标按下时播放列表的位移
        int scroll_offset{};            //当前播放列表滚动的位移
        CRect scrollbar_rect{};         //滚动条的位置
        CRect scrollbar_handle_rect;    //滚动条把手的位置
        bool scrollbar_hover{};         //鼠标指向滚动条
        bool scrollbar_handle_pressed{};    //滚动条把手被按下
        int scroll_handle_length_comp{};    //计算滚动条把手长度时的补偿量

    private:
        void UpdateSmoothScroll();          //每帧绘制前推进平滑滚动动画

        bool last_hover{};

        bool smooth_scrolling{};            //是否正在进行平滑滚动动画
        double smooth_offset{};             //平滑滚动动画中的当前位移（浮点数，避免每帧取整导致动画停滞）
        int smooth_target{};                //平滑滚动的目标位移
        int smooth_written_offset{};        //上一帧动画写入scroll_offset的值，若scroll_offset被其他操作（拖动、定位等）改变则取消动画
        std::chrono::steady_clock::time_point smooth_last_time;
        static std::atomic<long long> smooth_scroll_until;   //平滑滚动动画预计结束的时间（steady_clock，毫秒）
    };
}

