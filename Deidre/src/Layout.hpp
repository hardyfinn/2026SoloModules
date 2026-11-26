// Panel positions in mm. res/make_panel.py has the same numbers.
#pragma once
namespace deidre {
namespace lay {
static const float W = 284.48f;  // 56 HP
// left column
static const float BPM_X = 15.f, BPM_Y = 27.f;
static const float RUN_X = 35.f, RUN_Y = 22.f, RESET_Y = 37.f;
static const float COL_A = 9.f, COL_B = 22.f, COL_C = 35.f;
static const float ROW_BTN = 50.f, ROW_BANK = 68.f, ROW_IN1 = 88.f, ROW_IN2 = 107.f;
// screen
static const float SX = 48.f, SY = 12.f, SW = 120.f, SH = 72.f;
static const float PAGE_X0 = 56.f, PAGE_DX = 14.4f, PAGE_Y = 90.f;
static const float DATA_X = 160.f, DATA_Y = 91.f;
// knob bank (follows the selected output)
static const float BX0 = 172.f, BY0 = 12.f, BH = 17.2f, KPITCH = 20.3f;
static const float KCX0 = 190.65f;
inline float knobX(int c) { return KCX0 + c * KPITCH; }
inline float knobY(int r) { return BY0 + r * BH + 6.6f; }
// outputs row
static const float OX0 = 48.f, OY = 101.f, OCP = 26.81f;
inline float outC(int i) { return 67.f + i * OCP; }
}
}
