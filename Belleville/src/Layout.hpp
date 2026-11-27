// Panel positions in mm, the same numbers as res/make_panels.py
#pragma once
namespace lay {
static const float H = 128.5f;
static const float DISP_X = 5.f, DISP_Y = 14.5f;
static const float S_W = 24 * 5.08f;
static const float S_COLS[4] = {S_W / 2 - 40.65f, S_W / 2 - 13.55f, S_W / 2 + 13.55f, S_W / 2 + 40.65f};
static const float S_DISP_H = 22.f;
static const float S_ROWS[3] = {45.f, 62.5f, 80.f};
static const float T_W = 20 * 5.08f;
static const float T_COLS[3] = {T_W / 2 - 28.8f, T_W / 2, T_W / 2 + 28.8f};
static const float T_JCOLS[4] = {T_W / 2 - 35.4f, T_W / 2 - 11.8f, T_W / 2 + 11.8f, T_W / 2 + 35.4f};
static const float T_DISP_H = 18.f;
static const float T_ROWS[3] = {42.f, 59.5f, 77.5f};
static const float Y_IN = 102.f, Y_OUT = 116.5f;
static const float LINK_X_FROM_RIGHT = 6.3f, LINK_Y = 12.2f;
}
