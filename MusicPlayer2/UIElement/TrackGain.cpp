#include "stdafx.h"
#include "TrackGain.h"
#include "Player.h"
#include "SongDataManager.h"
#include "Slider.h"
#include "ElementFactory.h"

void UiElement::TrackGain::Draw()
{
    CalculateRect();

    //面板显示期间切换了曲目，则直接关闭面板（新曲目打开时已应用其自己的增益）
    if (m_show_popup && !(m_song == CPlayer::GetInstance().GetCurrentSongInfo()))
        m_show_popup = false;

    m_btn.enable = IsEnable();
    ui->DrawUIButton(rect, m_btn, ui->GetBtnIconType(CPlayerUIBase::BTN_TRACK_GAIN));

    Element::Draw();
}

void UiElement::TrackGain::DrawTopMost()
{
    if (!m_show_popup || !IsEnable())
        return;

    //计算面板的位置（显示在按钮上方）
    const CSize popup_size(ui->DPI(240), ui->DPI(72));
    rect_popup.top = rect.top - ui->DPI(4) - popup_size.cy;
    rect_popup.left = rect.left - (popup_size.cx - rect.Width()) / 2;
    rect_popup.right = rect_popup.left + popup_size.cx;
    rect_popup.bottom = rect_popup.top + popup_size.cy;
    CRect draw_rect = ui->GetClientDrawRect();
    CCommon::RestrictRectRange(rect_popup, draw_rect);

    //绘制背景
    ui->DrawRectangle(rect_popup, ui->GetUIColors().color_panel_back);

    CRect rect_content = rect_popup;
    rect_content.DeflateRect(ui->DPI(8), ui->DPI(8));

    //第一行：滑动条和增益文本
    CRect rect_row{ rect_content };
    rect_row.bottom = rect_row.top + ui->DPI(24);
    CString str_gain;
    str_gain.Format(_T("%+.1f dB"), gain_slider->GetCurPos() / 2.0f);
    CRect rect_text{ rect_row };
    rect_text.left = rect_text.right - ui->GetDrawer().GetTextExtent(L"+15.0 dB").cx;
    ui->GetDrawer().DrawWindowText(rect_text, str_gain, ui->GetUIColors().color_text);
    CRect rect_slider{ rect_row };
    rect_slider.right = rect_text.left - ui->DPI(8);
    gain_slider->SetRect(rect_slider);
    gain_slider->Draw();

    //第二行：取消和应用按钮（靠右）
    const std::wstring str_apply = theApp.m_str_table.LoadText(L"TXT_APPLY");
    const std::wstring str_cancel = theApp.m_str_table.LoadText(L"TXT_CANCEL");
    CRect rect_btn{ rect_content };
    rect_btn.top = rect_btn.bottom - ui->DPI(26);
    rect_btn.left = rect_btn.right - ui->DPI(40) - ui->GetDrawer().GetTextExtent(str_apply.c_str()).cx;
    ui->DrawUIButton(rect_btn, m_btn_apply, IconMgr::IT_Ok, false, str_apply, 9, false, Alignment::CENTER, true);
    rect_btn.right = rect_btn.left - ui->DPI(4);
    rect_btn.left = rect_btn.right - ui->DPI(40) - ui->GetDrawer().GetTextExtent(str_cancel.c_str()).cx;
    ui->DrawUIButton(rect_btn, m_btn_cancel, IconMgr::IT_Cancel, false, str_cancel, 9, false, Alignment::CENTER, true);
}

void UiElement::TrackGain::InitComplete()
{
    CElementFactory factory;
    gain_slider = std::dynamic_pointer_cast<Slider>(factory.CreateElement("slider", ui));
    gain_slider->SetRange(-30, 30);     //-15dB~15dB，步长0.5dB
    gain_slider->SetPosChangedTrigger([](UiElement::Slider* sender) {
        //拖动时实时预览
        CPlayer::GetInstance().SetTrackGain(sender->GetCurPos() / 2.0f);
    });
}

bool UiElement::TrackGain::LButtonUp(CPoint point)
{
    bool pressed = m_btn.pressed;
    m_btn.pressed = false;
    if (pressed && rect.PtInRect(point) && IsEnable() && IsShown())
    {
        ShowPopup();
        return true;
    }
    return false;
}

bool UiElement::TrackGain::LButtonDown(CPoint point)
{
    if (rect.PtInRect(point) && IsEnable() && IsShown())
        m_btn.pressed = true;
    return m_btn.pressed;
}

bool UiElement::TrackGain::MouseMove(CPoint point)
{
    m_btn.hover = (rect.PtInRect(point));
    if (m_btn.hover)
        ui->UpdateMouseToolTipPosition(CPlayerUIBase::BTN_TRACK_GAIN, rect);
    return false;
}

bool UiElement::TrackGain::MouseLeave()
{
    Element::MouseLeave();
    m_btn.hover = false;
    m_btn.pressed = false;
    gain_slider->MouseLeave();
    return false;
}

void UiElement::TrackGain::HideTooltip()
{
    ui->UpdateMouseToolTipPosition(CPlayerUIBase::BTN_TRACK_GAIN, CRect());
}

bool UiElement::TrackGain::GlobalLButtonUp(CPoint point)
{
    if (!m_show_popup || !IsShown() || !IsEnable())
        return false;

    bool rtn = false;
    if (rect_popup.PtInRect(mouse_pressed_point))
    {
        gain_slider->LButtonUp(point);
        if (m_btn_apply.pressed && m_btn_apply.rect.PtInRect(point))
            ClosePopup(true);
        else if (m_btn_cancel.pressed && m_btn_cancel.rect.PtInRect(point))
            ClosePopup(false);
        rtn = true;
    }
    else
    {
        //点击面板以外的地方视为取消
        ClosePopup(false);
    }
    m_btn_apply.pressed = false;
    m_btn_cancel.pressed = false;
    if (!rect_popup.PtInRect(point))
        gain_slider->MouseLeave();
    return rtn;
}

bool UiElement::TrackGain::GlobalLButtonDown(CPoint point)
{
    mouse_pressed_point = point;
    if (!m_show_popup || !IsShown() || !IsEnable())
        return false;

    if (rect_popup.PtInRect(point))
    {
        gain_slider->LButtonDown(point);
        m_btn_apply.pressed = m_btn_apply.rect.PtInRect(point);
        m_btn_cancel.pressed = m_btn_cancel.rect.PtInRect(point);
    }
    return true;
}

bool UiElement::TrackGain::GlobalMouseMove(CPoint point)
{
    if (!m_show_popup || !IsShown() || !IsEnable())
        return false;

    gain_slider->MouseMove(point);
    m_btn_apply.hover = m_btn_apply.rect.PtInRect(point);
    m_btn_cancel.hover = m_btn_cancel.rect.PtInRect(point);
    return true;
}

void UiElement::TrackGain::ShowPopup()
{
    const SongInfo& cur_song = CPlayer::GetInstance().GetCurrentSongInfo();
    if (cur_song.file_path.empty())
        return;
    m_song = cur_song;
    m_original_gain = CSongDataManager::GetInstance().GetSongInfo3(m_song).track_gain;
    gain_slider->SetCurPos(static_cast<int>(std::lround(m_original_gain * 2)));
    m_btn.hover = false;
    HideTooltip();
    m_show_popup = true;
}

void UiElement::TrackGain::ClosePopup(bool apply)
{
    m_show_popup = false;
    if (apply)
    {
        SongInfo song_info{ CSongDataManager::GetInstance().GetSongInfo3(m_song) };
        song_info.track_gain = gain_slider->GetCurPos() / 2.0f;
        CSongDataManager::GetInstance().AddItem(song_info);
    }
    else if (m_song == CPlayer::GetInstance().GetCurrentSongInfo())
    {
        CPlayer::GetInstance().SetTrackGain(m_original_gain);
    }
}
