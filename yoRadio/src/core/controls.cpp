#include "Arduino.h"
#include "controls.h"
#include "options.h"
#include "config.h"
#include "player.h"
#include "display.h"
#include "network.h"
#include "netserver.h"
#include "../yoEncoder/yoEncoder.h"

long encOldPosition = 0;
long enc2OldPosition = 0;
int lpId = -1;

/*
 * HW-724 CUSTOM ROTARY CONTROL
 *
 * Normal rotation:
 *   Clockwise      -> NEXT station / channel DOWN
 *   Anti-clockwise -> PREVIOUS station / channel UP
 *
 * Short press:
 *   PLAY <-> STOP
 *
 * Long press:
 *   No encoder volume mode; long press does not alter station browsing.
 *
 * Encoder pins are defined in myoptions.h.
 */
// HW-724 playlist browser state; rotary volume mode is disabled.
static bool stationBrowseActive = false;
static unsigned long lastEncoderUiAction = 0;
static const unsigned long ENCODER_UI_GAP_MS = 110;

#define ISPUSHBUTTONS (BTN_LEFT!=255 || BTN_CENTER!=255 || BTN_RIGHT!=255 || \
                       ENC_BTNB!=255 || BTN_UP!=255 || BTN_DOWN!=255 || \
                       ENC2_BTNB!=255 || BTN_MODE!=255)

#if ISPUSHBUTTONS
#include "../OneButton/OneButton.h"

OneButton button[] = {
  {BTN_LEFT,   true, BTN_INTERNALPULLUP},
  {BTN_CENTER, true, BTN_INTERNALPULLUP},
  {BTN_RIGHT,  true, BTN_INTERNALPULLUP},
  {ENC_BTNB,   true, ENC_INTERNALPULLUP},
  {BTN_UP,     true, BTN_INTERNALPULLUP},
  {BTN_DOWN,   true, BTN_INTERNALPULLUP},
  {ENC2_BTNB,  true, ENC2_INTERNALPULLUP},
  {BTN_MODE,   true, BTN_INTERNALPULLUP}
};

constexpr uint8_t nrOfButtons = sizeof(button) / sizeof(button[0]);
#endif

#if ENC_HALFQUARD==false
#define ENCODER_STEPS 4
#elif ENC_HALFQUARD==true
#define ENCODER_STEPS 2
#elif ENC_HALFQUARD==255
#define ENCODER_STEPS 1
#endif

#if ENC2_HALFQUARD==false
#define ENCODER2_STEPS 4
#elif ENC2_HALFQUARD==true
#define ENCODER2_STEPS 2
#elif ENC2_HALFQUARD==255
#define ENCODER2_STEPS 1
#endif

#if (ENC_BTNL!=255 && ENC_BTNR!=255) || (ENC2_BTNL!=255 && ENC2_BTNR!=255)

#if (ENC_BTNL!=255 && ENC_BTNR!=255)
yoEncoder encoder =
    yoEncoder(ENC_BTNL, ENC_BTNR, ENCODER_STEPS, ENC_INTERNALPULLUP);
#endif

#if (ENC2_BTNL!=255 && ENC2_BTNR!=255)
yoEncoder encoder2 =
    yoEncoder(ENC2_BTNL, ENC2_BTNR, ENCODER2_STEPS, ENC2_INTERNALPULLUP);
#endif

#endif

#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
#include "touchscreen.h"
TouchScreen touchscreen;
#endif

#if ENC_BTNL!=255
void IRAM_ATTR readEncoderISR()
{
  if ((SDC_CS==255 && display.mode()==LOST) || display.mode()==UPDATING)
    return;

  encoder.readEncoder_ISR();
}
#endif

#if ENC2_BTNL!=255
void IRAM_ATTR readEncoder2ISR()
{
  if ((SDC_CS==255 && display.mode()==LOST) || display.mode()==UPDATING)
    return;

  encoder2.readEncoder_ISR();
}
#endif

void initControls()
{
#if ENC_BTNL!=255
  encoder.begin();
  encoder.setup(readEncoderISR);
  encoder.setBoundaries(0, 254, true);
  encoder.setAcceleration(config.store.encacc);
#endif

#if ENC2_BTNL!=255
  encoder2.begin();
  encoder2.setup(readEncoder2ISR);
  encoder2.setBoundaries(0, 254, true);
  encoder2.setAcceleration(config.store.encacc);
#endif

#if ISPUSHBUTTONS
  for (int i = 0; i < nrOfButtons; i++)
  {
    if ((i == 0 && BTN_LEFT == 255) ||
        (i == 1 && BTN_CENTER == 255) ||
        (i == 2 && BTN_RIGHT == 255) ||
        (i == 3 && ENC_BTNB == 255) ||
        (i == 4 && BTN_UP == 255) ||
        (i == 5 && BTN_DOWN == 255) ||
        (i == 6 && ENC2_BTNB == 255) ||
        (i == 7 && BTN_MODE == 255))
      continue;

    button[i].attachClick([](void* p) {
      onBtnClick((int)p);
    }, (void*)i);

    button[i].attachDoubleClick([](void* p) {
      onBtnDoubleClick((int)p);
    }, (void*)i);

    button[i].attachLongPressStart([](void* p) {
      onBtnLongPressStart((int)p);
    }, (void*)i);

    button[i].attachLongPressStop([](void* p) {
      onBtnLongPressStop((int)p);
    }, (void*)i);

    button[i].setClickTicks(BTN_CLICK_TICKS);
    button[i].setPressTicks(BTN_PRESS_TICKS);
  }
#endif

#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
  touchscreen.init();
#endif

}

void loopControls()
{
  if (display.mode()==UPDATING || display.mode()==SDCHANGE)
    return;

  if (SDC_CS==255 && display.mode()==LOST)
    return;

#if ENC_BTNL!=255
  encoder1Loop();
#endif

#if ENC2_BTNL!=255
  encoder2Loop();
#endif

#if ISPUSHBUTTONS
  for (unsigned i = 0; i < nrOfButtons; i++)
  {
    if ((i == 0 && BTN_LEFT == 255) ||
        (i == 1 && BTN_CENTER == 255) ||
        (i == 2 && BTN_RIGHT == 255) ||
        (i == 3 && ENC_BTNB == 255) ||
        (i == 4 && BTN_UP == 255) ||
        (i == 5 && BTN_DOWN == 255) ||
        (i == 6 && ENC2_BTNB == 255) ||
        (i == 7 && BTN_MODE == 255))
      continue;

    button[i].tick();

    if (lpId >= 0)
    {
      if (DSP_MODEL == DSP_DUMMY && (lpId == 4 || lpId == 5))
        continue;

      onBtnDuringLongPress(lpId);
    }
  }
#endif

#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
  if (network.status == CONNECTED || network.status == SDREADY)
    touchscreen.loop();
#endif
}

#if ENC_BTNL!=255 || ENC2_BTNL!=255

void encodersLoop(yoEncoder *enc, bool first)
{
  if (network.status != CONNECTED && network.status != SDREADY)
    return;
  if (display.mode() == LOST || display.mode() == UPDATING)
    return;

  int8_t encoderDelta = enc->encoderChanged();
  if (encoderDelta == 0)
    return;

  if (first)
  {
    // Pace OLED updates so rotary bursts do not overload the display queue.
    unsigned long now = millis();
    if (now - lastEncoderUiAction < ENCODER_UI_GAP_MS)
      return;
    lastEncoderUiAction = now;

    int stationCount = (int)config.store.countStation;
    if (stationCount < 1)
      return;

    // Enter station browser once. Reset stale queued draw requests so the
    // display processes the mode change and current selection in order.
    if (!stationBrowseActive)
    {
      stationBrowseActive = true;
      display.resetQueue();
      display.putRequest(NEWMODE, STATIONS);
      display.currentPlItem = config.lastStation();
    }

    int station = (int)display.currentPlItem + (encoderDelta > 0 ? 1 : -1);
    if (station < 1) station = stationCount;
    if (station > stationCount) station = 1;

    display.currentPlItem = (uint16_t)station;
    display.putRequest(DRAWPLAYLIST);
    return;
  }

  // Optional second encoder retains previous/next station behavior.
  if (encoderDelta > 0)
    player.next();
  else
    player.prev();
}

#endif

#if ENC_BTNL!=255
void encoder1Loop()
{
  encodersLoop(&encoder, true);
}
#endif

#if ENC2_BTNL!=255
void encoder2Loop()
{
  encodersLoop(&encoder2, false);
}
#endif

/*
 * Long press START:
 * HW-724 encoder has no volume-control mode.
 */
void onBtnLongPressStart(int id)
{
  switch ((controlEvt_e)id)
  {
    case EVT_ENCBTNB:
      // Hold does not activate volume control.
      break;

    case EVT_BTNLEFT:
    case EVT_BTNRIGHT:
    case EVT_BTNUP:
    case EVT_BTNDOWN:
      lpId = id;
      break;

    case EVT_BTNMODE:
      display.putRequest(NEWMODE, SLEEPING);
      break;

    default:
      break;
  }
}

/*
 * Long press STOP:
 * No encoder volume mode to leave.
 */
void onBtnLongPressStop(int id)
{
  switch ((controlEvt_e)id)
  {
    case EVT_ENCBTNB:
      // Rotary volume mode is disabled.
      break;

    case EVT_BTNLEFT:
    case EVT_BTNRIGHT:
    case EVT_BTNUP:
    case EVT_BTNDOWN:
      lpId = -1;
      break;

    case EVT_BTNMODE:
      config.doSleepW();
      break;

    default:
      break;
  }
}

unsigned long lpdelay = 0;

boolean checklpdelay(int m, unsigned long &tstamp)
{
  if (millis() - tstamp > (unsigned long)m)
  {
    tstamp = millis();
    return true;
  }

  return false;
}

void onBtnDuringLongPress(int id)
{
  if (network.status != CONNECTED && network.status != SDREADY)
    return;

  if (!checklpdelay(BTN_LONGPRESS_LOOP_DELAY, lpdelay))
    return;

  switch ((controlEvt_e)id)
  {
    case EVT_BTNLEFT:
      controlsEvent(false);
      break;

    case EVT_BTNRIGHT:
      controlsEvent(true);
      break;

    case EVT_BTNUP:
    case EVT_BTNDOWN:
      if (display.mode() == PLAYER)
        display.putRequest(NEWMODE, STATIONS);

      if (display.mode() == STATIONS)
        controlsEvent(id == EVT_BTNDOWN);
      break;

    default:
      break;
  }
}

/*
 * Kept for compatibility with the rest of yoRadio.
 * The HW-724 encoder itself does NOT use this function for normal rotation.
 */
void controlsEvent(bool toRight, int8_t volDelta)
{
  if (display.mode() == NUMBERS)
  {
    display.numOfNextStation = 0;
    display.putRequest(NEWMODE, PLAYER);
  }

  if (display.mode() != STATIONS)
  {
#if !defined(DUMMYDISPLAY) || defined(USE_NEXTION)
    display.putRequest(NEWMODE, VOL);
#endif

    if (volDelta != 0)
    {
      int nv = (int)config.store.volume + volDelta;

      if (nv < 0)
        nv = 0;

      if (nv > 254)
        nv = 254;

      player.setVol((uint8_t)nv);
    }
    else
    {
      player.stepVol(toRight);
    }
  }

  if (display.mode() == STATIONS)
  {
    display.resetQueue();

    int p = toRight ? display.currentPlItem + 1
                    : display.currentPlItem - 1;

    if (p < 1)
      p = config.store.countStation;

    if (p > config.store.countStation)
      p = 1;

    display.currentPlItem = p;
    display.putRequest(DRAWPLAYLIST, p);
  }
}

/*
 * Short click:
 * Encoder button = PLAY / STOP.
 */
void onBtnClick(int id)
{
  controlEvt_e btnid = static_cast<controlEvt_e>(id);

  pm.on_btn_click(btnid);

  bool encoderButton =
      (btnid == EVT_ENCBTNB || btnid == EVT_ENC2BTNB);

  bool centerButton = (btnid == EVT_BTNCENTER);

  if (network.status != CONNECTED &&
      network.status != SDREADY &&
      !encoderButton &&
      !centerButton &&
      btnid != EVT_BTNMODE)
    return;

  switch (btnid)
  {
    case EVT_ENCBTNB:
    case EVT_BTNCENTER:
    {
      if (display.mode() == NUMBERS)
      {
        display.numOfNextStation = 0;
        display.putRequest(NEWMODE, PLAYER);
      }

      if (btnid == EVT_ENCBTNB && stationBrowseActive)
      {
        // Press confirms the highlighted station.
        int selectedStation = (int)display.currentPlItem;
        if (selectedStation >= 1 &&
            selectedStation <= (int)config.store.countStation)
        {
          player.sendCommand({PR_PLAY, selectedStation});
          stationBrowseActive = false;
          display.putRequest(NEWMODE, PLAYER);
        }
      }
      else if (display.mode() == PLAYER)
      {
        // Outside playlist browsing, press toggles play/stop.
        player.toggle();
      }

      if (display.mode() == SCREENSAVER ||
          display.mode() == SCREENBLANK)
      {
        display.putRequest(NEWMODE, PLAYER);
      }

      if (network.status == SOFT_AP || display.mode() == LOST)
      {
#ifdef USE_SD
        config.changeMode();
#endif
      }

      break;
    }

    case EVT_BTNLEFT:
      controlsEvent(false);
      break;

    case EVT_BTNRIGHT:
      controlsEvent(true);
      break;

    case EVT_BTNUP:
      player.prev();
      break;

    case EVT_BTNDOWN:
      player.next();
      break;

#ifdef USE_SD
    case EVT_BTNMODE:
      config.changeMode();
      break;
#endif

    default:
      break;
  }
}

/*
 * Double click:
 * HW-724 rotary double-click is intentionally disabled.
 * This prevents the old SD/WEB mode action.
 */
void onBtnDoubleClick(int id)
{
  if ((controlEvt_e)id == EVT_ENCBTNB ||
      (controlEvt_e)id == EVT_ENC2BTNB)
  {
    return;
  }

  if (display.mode() == SCREENSAVER ||
      display.mode() == SCREENBLANK)
  {
    display.putRequest(NEWMODE, PLAYER);
    return;
  }

  switch ((controlEvt_e)id)
  {
    case EVT_BTNLEFT:
      if (display.mode() == PLAYER &&
          (network.status == CONNECTED || network.status == SDREADY))
        player.prev();
      break;

    case EVT_BTNRIGHT:
      if (display.mode() == PLAYER &&
          (network.status == CONNECTED || network.status == SDREADY))
        player.next();
      break;

    default:
      break;
  }
}

void setIRTolerance(uint8_t tl)
{
  config.saveValue(&config.store.irtlp, tl);

#if IR_PIN!=255
  // IR support is unchanged when enabled.
  irrecv.setTolerance(config.store.irtlp);
#endif
}

void setEncAcceleration(uint16_t acc)
{
  config.saveValue(&config.store.encacc, acc);

#if ENC_BTNL!=255
  encoder.setAcceleration(config.store.encacc);
#endif

#if ENC2_BTNL!=255
  encoder2.setAcceleration(config.store.encacc);
#endif
}

void flipTS()
{
#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
  touchscreen.flip();
#endif
}
