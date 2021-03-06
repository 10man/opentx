/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "modeldata.h"

#include "eeprominterface.h"
#include "generalsettings.h"
#include "macros.h"
#include "radiodataconversionstate.h"
#include "helpers.h"
#include "adjustmentreference.h"

/*
 * TimerData
 */

void TimerData::convert(RadioDataConversionState & cstate)
{
  cstate.setComponent(tr("TMR"), 1);
  cstate.setSubComp(tr("Timer %1").arg(cstate.subCompIdx + 1));
  mode.convert(cstate);
}

bool TimerData::isEmpty()
{
  return (mode == RawSwitch(SWITCH_TYPE_TIMER_MODE, 0) && name[0] == '\0' && minuteBeep == 0 && countdownBeep == COUNTDOWN_SILENT && val == 0 && persistent == 0 /*&& pvalue == 0*/);
}

/*
 * ModelData
 */

ModelData::ModelData()
{
  clear();
}

ModelData::ModelData(const ModelData & src)
{
  *this = src;
}

ModelData & ModelData::operator = (const ModelData & src)
{
  memcpy(reinterpret_cast<void *>(this), &src, sizeof(ModelData));
  return *this;
}

ExpoData * ModelData::insertInput(const int idx)
{
  memmove(&expoData[idx + 1], &expoData[idx], (CPN_MAX_EXPOS - (idx + 1)) * sizeof(ExpoData));
  expoData[idx].clear();
  return &expoData[idx];
}

bool ModelData::isInputValid(const unsigned int idx) const
{
  for (int i = 0; i < CPN_MAX_EXPOS; i++) {
    const ExpoData * expo = &expoData[i];
    if (expo->mode == INPUT_MODE_NONE) break;
    if (expo->chn == idx)
      return true;
  }
  return false;
}

bool ModelData::hasExpos(uint8_t inputIdx) const
{
  for (int i = 0; i < CPN_MAX_EXPOS; i++) {
    const ExpoData & expo = expoData[i];
    if (expo.chn == inputIdx && expo.mode != INPUT_MODE_NONE) {
      return true;
    }
  }
  return false;
}

bool ModelData::hasMixes(uint8_t channelIdx) const
{
  channelIdx += 1;
  for (int i = 0; i < CPN_MAX_MIXERS; i++) {
    if (mixData[i].destCh == channelIdx) {
      return true;
    }
  }
  return false;
}

QVector<const ExpoData *> ModelData::expos(int input) const
{
  QVector<const ExpoData *> result;
  for (int i = 0; i < CPN_MAX_EXPOS; i++) {
    const ExpoData * ed = &expoData[i];
    if ((int)ed->chn == input && ed->mode != INPUT_MODE_NONE) {
      result << ed;
    }
  }
  return result;
}

QVector<const MixData *> ModelData::mixes(int channel) const
{
  QVector<const MixData *> result;
  for (int i = 0; i < CPN_MAX_MIXERS; i++) {
    const MixData * md = &mixData[i];
    if ((int)md->destCh == channel + 1) {
      result << md;
    }
  }
  return result;
}

void ModelData::removeInput(const int idx, bool clearName)
{
  unsigned int chn = expoData[idx].chn;

  memmove(&expoData[idx], &expoData[idx + 1], (CPN_MAX_EXPOS - (idx + 1)) * sizeof(ExpoData));
  expoData[CPN_MAX_EXPOS - 1].clear();

  //also remove input name if removing last line for this input
  if (clearName && !expos(chn).size())
    inputNames[chn][0] = 0;
}

void ModelData::clearInputs()
{
  for (int i = 0; i < CPN_MAX_EXPOS; i++)
    expoData[i].clear();

  //clear all input names
  if (getCurrentFirmware()->getCapability(VirtualInputs)) {
    for (int i = 0; i < CPN_MAX_INPUTS; i++) {
      inputNames[i][0] = 0;
    }
  }
}

void ModelData::clearMixes()
{
  for (int i = 0; i < CPN_MAX_MIXERS; i++)
    mixData[i].clear();
}

void ModelData::clear()
{
  memset(reinterpret_cast<void *>(this), 0, sizeof(ModelData));
  modelIndex = -1;  // an invalid index, this is managed by the TreeView data model
  moduleData[0].protocol = PULSES_OFF;
  moduleData[1].protocol = PULSES_OFF;
  moduleData[0].channelsCount = 8;
  moduleData[1].channelsStart = 0;
  moduleData[1].channelsCount = 8;
  moduleData[0].ppm.delay = 300;
  moduleData[1].ppm.delay = 300;
  moduleData[2].ppm.delay = 300;  //Trainer PPM
  for (int i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    flightModeData[i].clear(i);
  }
  for (int i = 0; i < CPN_MAX_GVARS; i++) {
    gvarData[i].clear();
  }
  clearInputs();
  clearMixes();
  for (int i = 0; i < CPN_MAX_CHNOUT; i++)
    limitData[i].clear();
  for (int i = 0; i < CPN_MAX_STICKS; i++)
    expoData[i].clear();
  for (int i = 0; i < CPN_MAX_LOGICAL_SWITCHES; i++)
    logicalSw[i].clear();
  for (int i = 0; i < CPN_MAX_SPECIAL_FUNCTIONS; i++)
    customFn[i].clear();
  for (int i = 0; i < CPN_MAX_CURVES; i++)
    curves[i].clear();
  for (int i = 0; i < CPN_MAX_TIMERS; i++)
    timers[i].clear();
  swashRingData.clear();
  frsky.clear();
  rssiAlarms.clear();
  for (unsigned i = 0; i < CPN_MAX_SENSORS; i++)
    sensorData[i].clear();

  static const uint8_t blob[] = { 0x4c, 0x61, 0x79, 0x6f, 0x75, 0x74, 0x32, 0x50, 0x31, 0x00, 0x4d, 0x6f, 0x64, 0x65, 0x6c, 0x42, 0x6d, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  memcpy(customScreenData[0], blob, sizeof(blob));
}

bool ModelData::isEmpty() const
{
  return !used;
}

void ModelData::setDefaultInputs(const GeneralSettings & settings)
{
  Board::Type board = getCurrentBoard();
  if (IS_ARM(board)) {
    for (int i = 0; i < CPN_MAX_STICKS; i++) {
      ExpoData * expo = &expoData[i];
      expo->chn = i;
      expo->mode = INPUT_MODE_BOTH;
      expo->srcRaw = settings.getDefaultSource(i);
      expo->weight = 100;
      strncpy(inputNames[i], Helpers::removeAccents(expo->srcRaw.toString(this)).toLatin1().constData(), sizeof(inputNames[i]) - 1);
    }
  }
}

void ModelData::setDefaultMixes(const GeneralSettings & settings)
{
  Board::Type board = getCurrentBoard();
  if (IS_ARM(board)) {
    setDefaultInputs(settings);
  }

  for (int i = 0; i < CPN_MAX_STICKS; i++) {
    MixData * mix = &mixData[i];
    mix->destCh = i + 1;
    mix->weight = 100;
    if (IS_ARM(board)) {
      mix->srcRaw = RawSource(SOURCE_TYPE_VIRTUAL_INPUT, i);
    }
    else {
      mix->srcRaw = RawSource(SOURCE_TYPE_STICK, i);
    }
  }
}

void ModelData::setDefaultValues(unsigned int id, const GeneralSettings & settings)
{
  clear();
  used = true;
  sprintf(name, "MODEL%02d", id+1);
  for (int i = 0; i < CPN_MAX_MODULES; i++) {
    moduleData[i].modelId = id + 1;
  }
  setDefaultMixes(settings);
}

int ModelData::getTrimValue(int phaseIdx, int trimIdx)
{
  int result = 0;
  for (int i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    FlightModeData & phase = flightModeData[phaseIdx];
    if (phase.trimMode[trimIdx] < 0) {
      return result;
    }
    else {
      if (phase.trimRef[trimIdx] == phaseIdx || phaseIdx == 0) {
        return result + phase.trim[trimIdx];
      }
      else {
        phaseIdx = phase.trimRef[trimIdx];
        if (phase.trimMode[trimIdx] != 0)
          result += phase.trim[trimIdx];
      }
    }
  }
  return 0;
}

bool ModelData::isGVarLinked(int phaseIdx, int gvarIdx)
{
  return flightModeData[phaseIdx].gvars[gvarIdx] > GVAR_MAX_VALUE;
}

bool ModelData::isGVarLinkedCircular(int phaseIdx, int gvarIdx)
{
  for (int i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    int val = flightModeData[phaseIdx].gvars[gvarIdx];
    if (phaseIdx == 0 || val <= GVAR_MAX_VALUE)
      return false;
    int nextPhase = val - (GVAR_MAX_VALUE + 1);
    if (nextPhase >= phaseIdx)
      nextPhase += 1;
    phaseIdx = nextPhase;
  }
  return true;
}

int ModelData::getGVarValue(int phaseIdx, int gvarIdx)
{
  for (int i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    int val = flightModeData[phaseIdx].gvars[gvarIdx];
    if (phaseIdx == 0 || val <= GVAR_MAX_VALUE)
      return val;
    int nextPhase = val - (GVAR_MAX_VALUE + 1);
    if (nextPhase >= phaseIdx)
      nextPhase += 1;
    phaseIdx = nextPhase;
  }
  return flightModeData[0].gvars[gvarIdx];  // circular linked so return FM0 value
}

bool ModelData::isREncLinked(int phaseIdx, int reIdx)
{
  return flightModeData[phaseIdx].rotaryEncoders[reIdx] > RENC_MAX_VALUE;
}

bool ModelData::isREncLinkedCircular(int phaseIdx, int reIdx)
{
  for (int i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    int val = flightModeData[phaseIdx].rotaryEncoders[reIdx];
    if (phaseIdx == 0 || val <= RENC_MAX_VALUE)
      return false;
    int nextPhase = val - (RENC_MAX_VALUE + 1);
    if (nextPhase >= phaseIdx)
      nextPhase += 1;
    phaseIdx = nextPhase;
  }
  return true;
}

int ModelData::getREncValue(int phaseIdx, int reIdx)
{
  for (int i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    int val = flightModeData[phaseIdx].rotaryEncoders[reIdx];
    if (phaseIdx == 0 || val <= RENC_MAX_VALUE)
      return val;
    int nextPhase = val - (RENC_MAX_VALUE + 1);
    if (nextPhase >= phaseIdx)
      nextPhase += 1;
    phaseIdx = nextPhase;
  }
  return flightModeData[0].rotaryEncoders[reIdx];  // circular linked so return FM0 value
}

void ModelData::setTrimValue(int phaseIdx, int trimIdx, int value)
{
  for (uint8_t i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    FlightModeData & phase = flightModeData[phaseIdx];
    int mode = phase.trimMode[trimIdx];
    int p = phase.trimRef[trimIdx];
    int & trim = phase.trim[trimIdx];
    if (mode < 0)
      return;
    if (p == phaseIdx || phaseIdx == 0) {
      trim = value;
      break;
    }
    else if (mode == 0) {
      phaseIdx = p;
    }
    else {
      trim = value - getTrimValue(p, trimIdx);
      if (trim < -500)
        trim = -500;
      if (trim > 500)
        trim = 500;
      break;
    }
  }
}

void ModelData::removeGlobalVar(int & var)
{
  if (var >= 126 && var <= 130)
    var = flightModeData[0].gvars[var - 126];
  else if (var <= -126 && var >= -130)
    var = - flightModeData[0].gvars[-126 - var];
}

ModelData ModelData::removeGlobalVars()
{
  ModelData result = *this;

  for (int i = 0; i < CPN_MAX_MIXERS; i++) {
    removeGlobalVar(mixData[i].weight);
    removeGlobalVar(mixData[i].curve.value);
    removeGlobalVar(mixData[i].sOffset);
  }

  for (int i  =0; i < CPN_MAX_EXPOS; i++) {
    removeGlobalVar(expoData[i].weight);
    removeGlobalVar(expoData[i].curve.value);
  }

  return result;
}

int ModelData::getChannelsMax(bool forceExtendedLimits) const
{
  if (forceExtendedLimits || extendedLimits)
    return IS_HORUS_OR_TARANIS(getCurrentBoard()) ? 150 : 125;
  else
    return 100;
}

bool ModelData::isAvailable(const RawSwitch & swtch) const
{
  unsigned index = abs(swtch.index) - 1;

  if (swtch.type == SWITCH_TYPE_VIRTUAL) {
    return logicalSw[index].func != LS_FN_OFF;
  }
  else if (swtch.type == SWITCH_TYPE_FLIGHT_MODE) {
    return index == 0 || flightModeData[index].swtch.type != SWITCH_TYPE_NONE;
  }
  else if (swtch.type == SWITCH_TYPE_SENSOR) {
    return strlen(sensorData[index].label) > 0;
  }
  else {
    return true;
  }
}

float ModelData::getGVarValuePrec(int phaseIdx, int gvarIdx)
{
  return getGVarValue(phaseIdx, gvarIdx) * gvarData[gvarIdx].multiplierGet();
}

void ModelData::convert(RadioDataConversionState & cstate)
{
  // Here we can add explicit conversions when moving from one board to another

  QString origin = QString(name);
  if (origin.isEmpty())
    origin = QString::number(cstate.modelIdx+1);
  cstate.setOrigin(tr("Model: ") % origin);

  cstate.setComponent("SET", 0);
  if (thrTraceSrc && (int)thrTraceSrc < cstate.fromBoard.getCapability(Board::Pots) + cstate.fromBoard.getCapability(Board::Sliders)) {
    cstate.setSubComp(tr("Throttle Source"));
    thrTraceSrc = RawSource(SOURCE_TYPE_STICK, (int)thrTraceSrc + 3).convert(cstate).index - 3;
  }

  for (int i = 0; i < CPN_MAX_TIMERS; i++) {
    timers[i].convert(cstate.withComponentIndex(i));
  }

  for (int i = 0; i < CPN_MAX_MIXERS; i++) {
    mixData[i].convert(cstate.withComponentIndex(i));
  }

  for (int i = 0; i < CPN_MAX_EXPOS; i++) {
    expoData[i].convert(cstate.withComponentIndex(i));
  }

  for (int i = 0; i < CPN_MAX_LOGICAL_SWITCHES; i++) {
    logicalSw[i].convert(cstate.withComponentIndex(i));
  }

  for (int i = 0; i < CPN_MAX_SPECIAL_FUNCTIONS; i++) {
    customFn[i].convert(cstate.withComponentIndex(i));
  }

  for (int i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    flightModeData[i].convert(cstate.withComponentIndex(i));
  }

  for (int i = 0; i < CPN_MAX_MODULES; i++) {
    moduleData[i].convert(cstate.withComponentIndex(i));
  }
}

#define MAX_REF_UPDATES 100

void ModelData::appendUpdateReferenceParams(const ReferenceUpdateType type, const ReferenceUpdateAction action, const int index1, const int index2, const int shift)
{
  if (updRefList) {
    //qDebug() << "Append parameters - type:" << type << " action:" << action << " index1:" << index1 << " index2:" << index2 << " shift:" << shift;
    if (updRefList->size() <= MAX_REF_UPDATES)
      updRefList->append(UpdateReferenceParams(type, action, index1, index2, shift));
    else
      qDebug() << "Warning: Update ignored as the list of updates exceeds " << MAX_REF_UPDATES;
  }
}

int ModelData::updateAllReferences(const ReferenceUpdateType type, const ReferenceUpdateAction action, const int index1, const int index2, const int shift)
{
  //Stopwatch s1("ModelData::updateAllReferences");
  //s1.report("Start");

  int loopcnt = 0;
  int updcnt = 0;
  updRefList = nullptr;
  QVector<UpdateReferenceParams> updRefParams;   //  declaring this variable in ModelData class crashes program on opening model file
  updRefList = &updRefParams;                    //  so declare pointer variable in ModelData class and pass it the address of the local array

  if (updRefList) {
    appendUpdateReferenceParams(type, action, index1, index2, shift);

    while (!updRefList->isEmpty())
      {
        if (++loopcnt > MAX_REF_UPDATES) {
          qDebug() << "Warning: Update iterations terminated early as the list exceeded " << MAX_REF_UPDATES;
          break;
        }
        //qDebug() << "Start of iteration:" << loopcnt;
        updcnt += updateReference();
        updRefList->removeFirst();
      }
  }

  qDebug() << "Iterations:" << loopcnt << " References updated:" << updcnt;
  //s1.report("Finish");

  return updcnt;
}

int ModelData::updateReference()
{
  UpdateReferenceParams p = updRefList->first();

  if (p.action < REF_UPD_ACT_CLEAR || p.action > REF_UPD_ACT_SWAP || p.type < REF_UPD_TYPE_CHANNEL || p.type > REF_UPD_TYPE_TIMER) {
    qDebug() << "Error - invalid parameters" << " > type:" << p.type << " action:" << p.action << " index1:" << p.index1 << " index2:" << p.index2 << " shift:" << p.shift;
    return 0;
  }

  memset(&updRefInfo, 0, sizeof(updRefInfo));
  updRefInfo.type = p.type;
  updRefInfo.action = p.action;
  updRefInfo.index1 = abs(p.index1);
  updRefInfo.index2 = abs(p.index2);
  updRefInfo.shift = p.shift;

  if ((updRefInfo.action == REF_UPD_ACT_SWAP && updRefInfo.index1 == updRefInfo.index2) || (updRefInfo.action == REF_UPD_ACT_SHIFT && updRefInfo.shift == 0)) {
    qDebug() << "Warning - nothing to do" << " > type:" << updRefInfo.type << " action:" << updRefInfo.action << " index1:" << updRefInfo.index1 << " index2:" << updRefInfo.index2 << " shift:" << updRefInfo.shift;
    return 0;
  }

  //Stopwatch s1("ModelData::updateReference");
  //s1.report("Start");

  Firmware *fw = getCurrentFirmware();

  switch (updRefInfo.type)
  {
    case REF_UPD_TYPE_CHANNEL:
      updRefInfo.srcType = SOURCE_TYPE_CH;
      updRefInfo.maxindex = fw->getCapability(Outputs);
      break;
    case REF_UPD_TYPE_CURVE:
      updRefInfo.maxindex = fw->getCapability(NumCurves);
      break;
    case REF_UPD_TYPE_FLIGHT_MODE:
      updRefInfo.swtchType = SWITCH_TYPE_FLIGHT_MODE;
      updRefInfo.maxindex = fw->getCapability(FlightModes);
      break;
    case REF_UPD_TYPE_GLOBAL_VARIABLE:
      updRefInfo.srcType = SOURCE_TYPE_GVAR;
      updRefInfo.maxindex = fw->getCapability(Gvars);
      break;
    case REF_UPD_TYPE_INPUT:
      updRefInfo.srcType = SOURCE_TYPE_VIRTUAL_INPUT;
      updRefInfo.maxindex = fw->getCapability(VirtualInputs);
      break;
    case REF_UPD_TYPE_LOGICAL_SWITCH:
      updRefInfo.srcType = SOURCE_TYPE_CUSTOM_SWITCH;
      updRefInfo.swtchType = SWITCH_TYPE_VIRTUAL;
      updRefInfo.maxindex = fw->getCapability(LogicalSwitches);
      break;
    case REF_UPD_TYPE_SCRIPT:
      updRefInfo.srcType = SOURCE_TYPE_LUA_OUTPUT;
      updRefInfo.maxindex = fw->getCapability(LuaScripts);
      break;
    case REF_UPD_TYPE_SENSOR:
      updRefInfo.srcType = SOURCE_TYPE_TELEMETRY;
      updRefInfo.swtchType = SWITCH_TYPE_SENSOR;
      updRefInfo.maxindex = fw->getCapability(Sensors);
      break;
    case REF_UPD_TYPE_TIMER:
      updRefInfo.srcType = SOURCE_TYPE_SPECIAL;
      updRefInfo.maxindex = fw->getCapability(Timers);
      //  rawsource index offset    TODO refactor timers so be own category
      updRefInfo.index1 += 2;
      updRefInfo.index2 += 2;
      updRefInfo.maxindex += 2;
      break;
    default:
      qDebug() << "Error - unhandled reference type:" << updRefInfo.type;
      return 0;
  }

  updRefInfo.maxindex--;  //  getCapabilities and constants are 1 based

  //qDebug() << "updRefInfo - type:" << updRefInfo.type << " action:" << updRefInfo.action << " index1:" << updRefInfo.index1 << " index2:" << updRefInfo.index2 << " shift:" << updRefInfo.shift;
  //qDebug() << "maxindex:" << updRefInfo.maxindex << "updRefInfo - srcType:" << updRefInfo.srcType << " swtchType:" << updRefInfo.swtchType;

  //s1.report("Initialise");

  for (int i = fw->getCapability(NumFirstUsableModule); i < fw->getCapability(NumModules); i++) {
    ModuleData *md = &moduleData[i];
    if (md->protocol != PULSES_OFF && md->failsafeMode == FAILSAFE_CUSTOM && md->hasFailsafes(fw))
      updateModuleFailsafes(md);
  }
  //s1.report("Modules");

  for (int i = 0; i < CPN_MAX_TIMERS; i++) {
    TimerData *td = &timers[i];
    if (!td->isModeOff()) {
      updateTimerMode(td->mode);
      if (td->isModeOff())
        appendUpdateReferenceParams(REF_UPD_TYPE_TIMER, REF_UPD_ACT_CLEAR, i);
    }
  }
  //s1.report("Timers");

  for (int i = 1; i < CPN_MAX_FLIGHT_MODES; i++) {  //  skip FM0 as switch not used
    FlightModeData *fmd = &flightModeData[i];
    if (fmd->swtch.isSet()) {
      updateSwitchRef(fmd->swtch);
      if(!fmd->swtch.isSet())
        appendUpdateReferenceParams(REF_UPD_TYPE_FLIGHT_MODE, REF_UPD_ACT_CLEAR, i);
    }
  }
  //s1.report("Flight Modes");

  for (int i = 0; i < CPN_MAX_EXPOS; i++) {
    ExpoData *ed = &expoData[i];
    if (!ed->isEmpty()) {
      updateSourceRef(ed->srcRaw);
      if (ed->srcRaw.isSet()) {
        updateSwitchRef(ed->swtch);
        updateCurveRef(ed->curve);
        updateAdjustRef(ed->weight);
        updateAdjustRef(ed->offset);
        updateFlightModeFlags(ed->flightModes);
      }
      else {
        unsigned int chnsave = ed->chn;
        removeInput(i);
        i--;
        if (!hasExpos(chnsave))
          appendUpdateReferenceParams(REF_UPD_TYPE_INPUT, REF_UPD_ACT_CLEAR, chnsave);
      }
    }
  }
  //s1.report("Inputs");

  for (int i = 0; i < CPN_MAX_MIXERS; i++) {
    MixData *md = &mixData[i];
    if (!md->isEmpty()) {
      updateDestCh(md);
      if (!md->isEmpty()) {
        updateSourceRef(md->srcRaw);
        if (md->srcRaw.isSet()) {
          updateSwitchRef(md->swtch);
          updateCurveRef(md->curve);
          updateAdjustRef(md->weight);
          updateAdjustRef(md->sOffset);
          updateFlightModeFlags(md->flightModes);
        }
        else {
          removeMix(i);
          i--;
        }
      }
    }
  }
  //s1.report("Mixes");

  for (int i = 0; i < CPN_MAX_CHNOUT; i++) {
    LimitData *ld = &limitData[i];
    if (!ld->isEmpty()) {
      updateAdjustRef(ld->min);
      updateAdjustRef(ld->max);
      updateAdjustRef(ld->offset);
      updateLimitCurveRef(ld->curve);
    }
  }
  //s1.report("Outputs");

  for (int i = 0; i < CPN_MAX_LOGICAL_SWITCHES; i++) {
    LogicalSwitchData *lsd = &logicalSw[i];
    if (!lsd->isEmpty()) {
      bool clearlsd = false;
      CSFunctionFamily family = lsd->getFunctionFamily();
      switch(family) {
        case LS_FAMILY_VOFS:
          updateSourceIntRef(lsd->val1);
          if (lsd->val1 == 0)
            clearlsd = true;
          break;
        case LS_FAMILY_STICKY:
        case LS_FAMILY_VBOOL:
          updateSwitchIntRef(lsd->val1);
          updateSwitchIntRef(lsd->val2);
          if (lsd->val1 == 0 && lsd->val2 == 0)
            clearlsd = true;
          break;
        case LS_FAMILY_EDGE:
          updateSwitchIntRef(lsd->val1);
          if (lsd->val1 == 0)
            clearlsd = true;
          break;
        case LS_FAMILY_VCOMP:
          updateSourceIntRef(lsd->val1);
          updateSourceIntRef(lsd->val2);
          if (lsd->val1 == 0 && lsd->val2 == 0)
            clearlsd = true;
          break;
        default:
          break;
      }
      if (clearlsd) {
        lsd->clear();
        appendUpdateReferenceParams(REF_UPD_TYPE_LOGICAL_SWITCH, REF_UPD_ACT_CLEAR, i);
      }
      else {
        updateSwitchIntRef(lsd->andsw);
      }
    }
  }
  //s1.report("Logical Switches");

  for (int i = 0; i < CPN_MAX_SPECIAL_FUNCTIONS; i++) {
    CustomFunctionData *cfd = &customFn[i];
    if (!cfd->isEmpty()) {
      updateAssignFunc(cfd);
      if (!cfd->isEmpty()) {
        updateSwitchRef(cfd->swtch);
        if (cfd->func == FuncVolume || cfd->func == FuncPlayValue || (cfd->func >= FuncAdjustGV1 && cfd->func <= FuncAdjustGVLast && (cfd->adjustMode == FUNC_ADJUST_GVAR_GVAR || cfd->adjustMode == FUNC_ADJUST_GVAR_SOURCE))) {
          updateSourceIntRef(cfd->param);
          if (cfd->param == 0)
            cfd->clear();
        }
      }
    }
  }
  //s1.report("Special Functions");

  if (fw->getCapability(Heli)) {
    updateSourceRef(swashRingData.aileronSource);
    updateSourceRef(swashRingData.collectiveSource);
    updateSourceRef(swashRingData.elevatorSource);
    //s1.report("Heli");
  }

  if (fw->getCapability(Telemetry)) {
    updateTelemetryRef(frsky.voltsSource);
    updateTelemetryRef(frsky.altitudeSource);
    updateTelemetryRef(frsky.currentSource);
    updateTelemetryRef(frsky.varioSource);
    for (int i = 0; i < fw->getCapability(TelemetryCustomScreens); i++) {
      switch(frsky.screens[i].type) {
        case TELEMETRY_SCREEN_BARS:
          for (int j = 0; j < fw->getCapability(TelemetryCustomScreensBars); j++) {
            FrSkyBarData *fbd = &frsky.screens[i].body.bars[j];
            updateSourceRef(fbd->source);
            if (!fbd->source.isSet()) {
              fbd->barMin = 0;
              fbd->barMax = 0;
            }
          }
          break;
        case TELEMETRY_SCREEN_NUMBERS:
          for (int j = 0; j < fw->getCapability(TelemetryCustomScreensLines); j++) {
            FrSkyLineData *fld = &frsky.screens[i].body.lines[j];
            for (int k = 0; k < fw->getCapability(TelemetryCustomScreensFieldsPerLine); k++) {
              updateSourceRef(fld->source[k]);
            }
          }
          break;
        default:
          break;
      }
    }
    //s1.report("Telemetry");
  }

  //  TODO maybe less risk to leave it up to the user??
  /*
  for (int i = 0; i < CPN_MAX_SENSORS; i++) {
    SensorData *sd = &sensorData[i];
    if (sd->isAvailable() && sd->type == SensorData::TELEM_TYPE_CALCULATED) {
    }
  }
  s1.report("Telemetry Sensors");
  */

  //  TODO needs lua incorporated into Companion as script needs to be parsed to determine if input field is source or value
  /*
  for (int i=0; i < CPN_MAX_SCRIPTS; i++) {
    ScriptData *sd = &scriptData[i];
    if (sd->filename[0] != '\0') {
      for (int j = 0; j < CPN_MAX_SCRIPT_INPUTS; j++) {
        //  get input parameters and for each one where type is SOURCE
        if(inputtype = "SOURCE")
          updateSourceIntRef(sd->inputs[j]);
      }
    }
  }
  s1.report("Custom Scripts");
  */

  //  TODO Horus CustomScreenData and TopBarData will need checking for updates but Companion does not current handle ie just data blobs refer modeldata.h

  qDebug() << "References updated this iteration:" << updRefInfo.updcnt;
  //s1.report("Finish");

  return updRefInfo.updcnt;
}

template <class R, typename T>
void ModelData::updateTypeIndexRef(R & curRef, const T type, const int idxAdj, const bool defClear, const int defType, const int defIndex)
{
  //qDebug() << "Raw value: " << curRef.toValue() << " String:" << curRef.toString() << " Type: " << curRef.type << " Index:" << curRef.index << " idxAdj:" << idxAdj << " defClear:" << defClear << " defType:" << defType << " defIndex:" << defIndex;
  if (!curRef.isSet() || curRef.type != type)
    return;

  R newRef;
  newRef.type = curRef.type;
  newRef.index = abs(curRef.index);

  switch (updRefInfo.action)
  {
    case REF_UPD_ACT_CLEAR:
      if (newRef.index != (updRefInfo.index1 + idxAdj))
        return;
      if (defClear)
        newRef.clear();
      else {
        newRef.type = (T)defType;
        newRef.index = defIndex;
      }
      break;
    case REF_UPD_ACT_SHIFT:
      if (newRef.index < (updRefInfo.index1 + idxAdj))
        return;

      newRef.index += updRefInfo.shift;

      if (newRef.index < (updRefInfo.index1 + idxAdj) || newRef.index > (updRefInfo.maxindex + idxAdj)) {
        if (defClear)
          newRef.clear();
        else {
          newRef.type = (T)defType;
          newRef.index = defIndex;
        }
      }
      break;
    case REF_UPD_ACT_SWAP:
      if (newRef.index == updRefInfo.index1 + idxAdj)
        newRef.index = updRefInfo.index2 + idxAdj;
      else if (newRef.index == updRefInfo.index2 + idxAdj)
        newRef.index = updRefInfo.index1 + idxAdj;
      break;
    default:
      qDebug() << "Error - unhandled action:" << updRefInfo.action;
      return;
  }

  if (curRef.type != newRef.type || abs(curRef.index) != newRef.index) {
    newRef.index = curRef.index < 0 ? -newRef.index : newRef.index;
    //qDebug() << "Updated reference: " << curRef.toString() << " -> " << newRef.toString();
    curRef = newRef;
    updRefInfo.updcnt++;
  }
}

template <class R, typename T>
void ModelData::updateTypeValueRef(R & curRef, const T type, const int idxAdj, const bool defClear, const int defType, const int defValue)
{
  //qDebug() << " String:" << curRef.toString() << " Type: " << curRef.type << " Value:" << curRef.value;
  if (!curRef.isSet() || curRef.type != type)
    return;

  R newRef;
  newRef.type = curRef.type;
  newRef.value = abs(curRef.value);

  switch (updRefInfo.action)
  {
    case REF_UPD_ACT_CLEAR:
      if (newRef.value != (updRefInfo.index1 + idxAdj))
        return;
      if (defClear)
        newRef.clear();
      else {
        newRef.type = (T)defType;
        newRef.value = defValue;
      }
      break;
    case REF_UPD_ACT_SHIFT:
      if (newRef.value < (updRefInfo.index1 + idxAdj))
        return;

      newRef.value += updRefInfo.shift;

      if (newRef.value < (updRefInfo.index1 + idxAdj) || newRef.value > (updRefInfo.maxindex + idxAdj)) {
        if (defClear)
          newRef.clear();
        else {
          newRef.type = (T)defType;
          newRef.value = defValue;
        }
      }
      break;
    case REF_UPD_ACT_SWAP:
      if (newRef.value == updRefInfo.index1 + idxAdj)
        newRef.value = updRefInfo.index2 + idxAdj;
      else if (newRef.value == updRefInfo.index2 + idxAdj)
        newRef.value = updRefInfo.index1 + idxAdj;
      break;
    default:
      qDebug() << "Error - unhandled action:" << updRefInfo.action;
      return;
  }

  if (curRef.type != newRef.type || abs(curRef.value) != newRef.value) {
    newRef.value = curRef.value < 0 ? -newRef.value : newRef.value;
    //qDebug() << "Updated reference: " << curRef.toString() << " -> " << newRef.toString();
    curRef = newRef;
    updRefInfo.updcnt++;
  }
}

void ModelData::updateCurveRef(CurveReference & crv)
{
  if (updRefInfo.type == REF_UPD_TYPE_GLOBAL_VARIABLE && (crv.type == CurveReference::CURVE_REF_DIFF || crv.type == CurveReference::CURVE_REF_EXPO))
    updateAdjustRef(crv.value);
  else if (updRefInfo.type == REF_UPD_TYPE_CURVE && crv.type == CurveReference::CURVE_REF_CUSTOM)
    updateTypeValueRef<CurveReference, CurveReference::CurveRefType>(crv, CurveReference::CURVE_REF_CUSTOM, 1);
}

void ModelData::updateLimitCurveRef(CurveReference & crv)
{
  CurveReference src = CurveReference(CurveReference::CURVE_REF_CUSTOM, crv.value);
  updateCurveRef(src);
  if (crv.value != src.value)
    crv.value = src.value;
}

void ModelData::updateAdjustRef(int & value)
{
  if (updRefInfo.type != REF_UPD_TYPE_GLOBAL_VARIABLE)
    return;

  AdjustmentReference adj = AdjustmentReference(value);
  updateTypeValueRef<AdjustmentReference, AdjustmentReference::AdjustRefType>(adj, AdjustmentReference::ADJUST_REF_GVAR, 1);
  if (value != adj.toValue())
    value = adj.toValue();
}

void ModelData::updateAssignFunc(CustomFunctionData * cfd)
{
  const int invalidateRef = -1;
  int newRef = (int)cfd->func;
  int idxAdj;

  switch (updRefInfo.type)
  {
    case REF_UPD_TYPE_CHANNEL:
      if(cfd->func < FuncOverrideCH1 || cfd->func > FuncOverrideCH32) //  TODO refactor to FuncOverrideCHLast
        return;
      idxAdj = FuncOverrideCH1;
      break;
    case REF_UPD_TYPE_GLOBAL_VARIABLE:
      if (cfd->func < FuncAdjustGV1 || cfd->func > FuncAdjustGVLast)
        return;
      idxAdj = FuncAdjustGV1;
      break;
    case REF_UPD_TYPE_TIMER:
      if (cfd->func < FuncSetTimer1 || cfd->func > FuncSetTimer3) //  TODO refactor to FuncSetTimerLast
        return;
      idxAdj = FuncSetTimer1 - 2;   //  reverse earlier offset requiured for rawsource
      break;
    default:
      return;
  }

  switch (updRefInfo.action)
  {
    case REF_UPD_ACT_CLEAR:
      if (newRef != (updRefInfo.index1 + idxAdj))
        return;
      newRef = invalidateRef;
      break;
    case REF_UPD_ACT_SHIFT:
      if (newRef < (updRefInfo.index1 + idxAdj))
        return;

      newRef += updRefInfo.shift;

      if (newRef < (updRefInfo.index1 + idxAdj) || newRef > (updRefInfo.maxindex + idxAdj))
        newRef = invalidateRef;
      break;
    case REF_UPD_ACT_SWAP:
      if (newRef == updRefInfo.index1 + idxAdj)
        newRef = updRefInfo.index2 + idxAdj;
      else if (newRef == updRefInfo.index2 + idxAdj)
        newRef = updRefInfo.index1 + idxAdj;
      break;
    default:
      qDebug() << "Error - unhandled action:" << updRefInfo.action;
      return;
  }

  if (newRef == invalidateRef) {
    cfd->clear();
    //qDebug() << "Function cleared";
    updRefInfo.updcnt++;
  }
  else if (cfd->func != (AssignFunc)newRef) {
    //qDebug() << "Updated reference:" << cfd->func << " -> " << newRef;
    cfd->func = (AssignFunc)newRef;
    updRefInfo.updcnt++;
  }
}

void ModelData::updateDestCh(MixData * md)
{
  if (updRefInfo.type != REF_UPD_TYPE_CHANNEL)
    return;

  const int invalidateRef = -1;
  const int idxAdj = 1;
  int newRef = md->destCh;
  switch (updRefInfo.action)
  {
    case REF_UPD_ACT_CLEAR:
      if (newRef != (updRefInfo.index1 + idxAdj))
        return;
      newRef = invalidateRef;
      break;
    case REF_UPD_ACT_SHIFT:
      if (newRef < (updRefInfo.index1 + idxAdj))
        return;

      newRef += updRefInfo.shift;

      if (newRef < (updRefInfo.index1 + idxAdj) || newRef > (updRefInfo.maxindex + idxAdj))
        newRef = invalidateRef;
      break;
    case REF_UPD_ACT_SWAP:
      if (newRef == updRefInfo.index1 + idxAdj)
        newRef = updRefInfo.index2 + idxAdj;
      else if (newRef == updRefInfo.index2 + idxAdj)
        newRef = updRefInfo.index1 + idxAdj;
      break;
    default:
      qDebug() << "Error - unhandled action:" << updRefInfo.action;
      return;
  }

  if (newRef == invalidateRef) {
    md->clear();
    //qDebug() << "Mix cleared";
    updRefInfo.updcnt++;
  }
  else if (md->destCh != static_cast<unsigned int>(newRef)) {
    //qDebug() << "Updated reference:" << md->destCh << " -> " << newRef;
    md->destCh = newRef;
    updRefInfo.updcnt++;
  }
}

void ModelData::updateFlightModeFlags(unsigned int & curRef)
{
  if (updRefInfo.type != REF_UPD_TYPE_FLIGHT_MODE || curRef == 0 /*all selected*/ || curRef == 511 /*all deselected*/)
   return;

  if (updRefInfo.index1 > CPN_MAX_FLIGHT_MODES || updRefInfo.index2 > CPN_MAX_FLIGHT_MODES)
    return;

  unsigned int newRef = curRef;
  bool flag[CPN_MAX_FLIGHT_MODES];
  bool f;

  int mask = 1;
  for (int i = 0; i < CPN_MAX_FLIGHT_MODES; i++) {
    flag[i] = curRef & mask;
    mask <<= 1;
  }

  switch (updRefInfo.action)
  {
    case REF_UPD_ACT_CLEAR:
      flag[updRefInfo.index1] = false;
      break;
    case REF_UPD_ACT_SHIFT:
        if(updRefInfo.shift < 0) {
          for (int i = updRefInfo.index1; i < CPN_MAX_FLIGHT_MODES; i++) {
            if (i - updRefInfo.shift <= updRefInfo.maxindex)
              flag[i] = flag[i - updRefInfo.shift];
            else
              flag[i] = false;
          }
        }
        else {
          for (int i = CPN_MAX_FLIGHT_MODES - 1; i >= updRefInfo.index1; i--) {
            if (i - updRefInfo.shift >= updRefInfo.index1)
              flag[i] = flag[i - updRefInfo.shift];
            else
              flag[i] = false;
          }
        }
      break;
    case REF_UPD_ACT_SWAP:
      f = flag[updRefInfo.index1];
      flag[updRefInfo.index1] = flag[updRefInfo.index2];
      flag[updRefInfo.index2] = f;
      break;
    default:
      qDebug() << "Error - unhandled action:" << updRefInfo.action;
      return;
  }

  newRef = 0;
  for (int i = CPN_MAX_FLIGHT_MODES - 1; i >= 0 ; i--) {
    if (flag[i])
      newRef++;
    newRef <<= 1;
  }
  newRef >>= 1;

  if (curRef != newRef) {
    //qDebug() << "Updated reference:" << curRef << " -> " << newRef;
    curRef = newRef;
    updRefInfo.updcnt++;
  }
}

void ModelData::updateTelemetryRef(unsigned int & curRef)
{
  if (updRefInfo.type != REF_UPD_TYPE_SENSOR)
    return;

  const int idxAdj = 1;
  int newRef = curRef;
  switch (updRefInfo.action)
  {
    case REF_UPD_ACT_CLEAR:
      if (newRef != (updRefInfo.index1 + idxAdj))
        return;
      newRef = 0;
      break;
    case REF_UPD_ACT_SHIFT:
      if (newRef < (updRefInfo.index1 + idxAdj))
        return;

      newRef += updRefInfo.shift;

      if (newRef < (updRefInfo.index1 + idxAdj) || newRef > (updRefInfo.maxindex + idxAdj))
        newRef = 0;
      break;
    case REF_UPD_ACT_SWAP:
      if (newRef == updRefInfo.index1 + idxAdj)
        newRef = updRefInfo.index2 + idxAdj;
      else if (newRef == updRefInfo.index2 + idxAdj)
        newRef = updRefInfo.index1 + idxAdj;
      break;
    default:
      qDebug() << "Error - unhandled action:" << updRefInfo.action;
      return;
  }

  if (curRef != static_cast<unsigned int>(newRef)) {
    //qDebug() << "Updated reference:" << curRef << " -> " << newRef;
    curRef = newRef;
    updRefInfo.updcnt++;
  }
}

void ModelData::updateModuleFailsafes(ModuleData * md)
{
  if (updRefInfo.type != REF_UPD_TYPE_CHANNEL)
    return;

  bool updated = false;

  switch (updRefInfo.action) {
    case REF_UPD_ACT_CLEAR:
      break;
    case REF_UPD_ACT_SHIFT:
      if (updRefInfo.shift == 0 || updRefInfo.index1 < 0 || updRefInfo.index1 > CPN_MAX_CHNOUT - 1)
        return;

      if (updRefInfo.shift > 0) {
        for (int i = CPN_MAX_CHNOUT - 1; i > updRefInfo.index1; i--) {
          limitData[i].failsafe = limitData[i - 1].failsafe;
        }
        limitData[updRefInfo.index1].failsafe = 0;
      }
      else {
        for (int i = updRefInfo.index1 + 1; i < CPN_MAX_CHNOUT - 1; i++) {
          limitData[i - 1].failsafe = limitData[i].failsafe;
        }
        limitData[CPN_MAX_CHNOUT - 1].failsafe = 0;
      }
      updated = true;
      break;
    case REF_UPD_ACT_SWAP:
      int tmp;
      if (updRefInfo.index1 >= 0 && updRefInfo.index1 < CPN_MAX_CHNOUT) {
        updated = true;
        tmp = limitData[updRefInfo.index1].failsafe;
        if (updRefInfo.index2 >= 0 && updRefInfo.index2 < CPN_MAX_CHNOUT)
          limitData[updRefInfo.index1].failsafe = limitData[updRefInfo.index2].failsafe;
        else
          limitData[updRefInfo.index1].failsafe = 0;
      }
      else
        tmp = 0;
      if (updRefInfo.index2 >= 0 && updRefInfo.index2 < CPN_MAX_CHNOUT) {
        updated = true;
        limitData[updRefInfo.index2].failsafe = tmp;
      }
      break;
    default:
      qDebug() << "Error - unhandled action:" << updRefInfo.action;
      return;
  }

  if (updated) {
    //qDebug() << "Updated module failsafes";
    updRefInfo.updcnt++;
  }
}

int ModelData::linkedFlightModeIndexToValue(const int phaseIdx, const int useFmIdx, const int maxOwnValue)
{
  int val;

  if (phaseIdx == useFmIdx || phaseIdx < 0 || phaseIdx > (CPN_MAX_FLIGHT_MODES - 1) || useFmIdx < 0 || useFmIdx > (CPN_MAX_FLIGHT_MODES - 1))
    val = flightModeData[phaseIdx].linkedFlightModeZero(phaseIdx, maxOwnValue);
  else
    val = maxOwnValue + useFmIdx + (useFmIdx >= phaseIdx ? 0 : 1);

  return val;
}

int ModelData::linkedFlightModeValueToIndex(const int phaseIdx, const int val, const int maxOwnValue)
{
  int idx = val - maxOwnValue - 1;
  if (idx >= phaseIdx)
    idx += 1;
  return idx;
}

int ModelData::getGVarFlightModeIndex(const int phaseIdx, const int gvarIdx)
{
  if (!isGVarLinked(phaseIdx, gvarIdx))
    return -1;
  return (linkedFlightModeValueToIndex(phaseIdx, flightModeData[phaseIdx].gvars[gvarIdx], GVAR_MAX_VALUE));
}

void ModelData::setGVarFlightModeIndexToValue(const int phaseIdx, const int gvarIdx, const int useFmIdx)
{
  flightModeData[phaseIdx].gvars[gvarIdx] = linkedFlightModeIndexToValue(phaseIdx, useFmIdx, GVAR_MAX_VALUE);
}

int ModelData::getREncFlightModeIndex(const int phaseIdx, const int reIdx)
{
  if (!isREncLinked(phaseIdx, reIdx))
    return -1;
  return (linkedFlightModeValueToIndex(phaseIdx, flightModeData[phaseIdx].rotaryEncoders[reIdx], RENC_MAX_VALUE));
}

void ModelData::setREncFlightModeIndexToValue(const int phaseIdx, const int reIdx, const int useFmIdx)
{
  flightModeData[phaseIdx].rotaryEncoders[reIdx] = linkedFlightModeIndexToValue(phaseIdx, useFmIdx, RENC_MAX_VALUE);
}

bool ModelData::isExpoParent(const int index)
{
  const ExpoData &ed = expoData[index];
  const QVector<const ExpoData *> chexpos = expos(ed.chn);
  return chexpos.constFirst() == &ed;
}

bool ModelData::isExpoChild(const int index)
{
  const ExpoData &ed = expoData[index];
  const QVector<const ExpoData *> chexpos = expos(ed.chn);
  return chexpos.constFirst() != &ed;
}

bool ModelData::hasExpoChildren(const int index)
{
  const ExpoData &ed = expoData[index];
  const QVector<const ExpoData *> chexpos = expos(ed.chn);
  return chexpos.constFirst() == &ed && chexpos.constLast() != &ed;
}

bool ModelData::hasExpoSiblings(const int index)
{
  const ExpoData &ed = expoData[index];
  const QVector<const ExpoData *> chexpos = expos(ed.chn);
  return !isExpoParent(index) && chexpos.size() > 2;
}

void ModelData::removeMix(const int idx)
{
  memmove(&mixData[idx], &mixData[idx + 1], (CPN_MAX_MIXERS - (idx + 1)) * sizeof(MixData));
  mixData[CPN_MAX_MIXERS - 1].clear();
}
