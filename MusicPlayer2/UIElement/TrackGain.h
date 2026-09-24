#pragma once
#include "UIElement/UIElement.h"

namespace UiElement
{
    class Slider;

    //曲目音量增益（不修改文件），点击后弹出调节面板
    class TrackGain : public Element
    {
    public:
        virtual void Draw() override;
        virtual void DrawTopMost() override;
        virtual void InitComplete() override;

        virtual bool LButtonUp(CPoint point) override;
        virtual bool LButtonDown(CPoint point) override;
        virtual bool MouseMove(CPoint point) override;
        virtual bool MouseLeave() override;
        virtual void HideTooltip() override;

        virtual bool GlobalLButtonUp(CPoint point) override;
        virtual bool GlobalLButtonDown(CPoint point) override;
        virtual bool GlobalMouseMove(CPoint point) override;

    protected:
        void ShowPopup();
        void ClosePopup(bool apply);

    protected:
        CPlayerUIBase::UIButton m_btn;
        CPlayerUIBase::UIButton m_btn_cancel;
        CPlayerUIBase::UIButton m_btn_apply;
        bool m_show_popup{ false };
        CRect rect_popup;
        std::shared_ptr<Slider> gain_slider{};  //单位为0.5dB
        SongInfo m_song;                //弹出面板时的曲目
        float m_original_gain{};        //弹出面板时曲目的增益，取消时恢复
        CPoint mouse_pressed_point;
    };
}
