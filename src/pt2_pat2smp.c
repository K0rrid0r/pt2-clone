// for finding memory leaks in debug mode with Visual Studio 
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include "pt2_header.h"
#include "pt2_helpers.h"
#include "pt2_visuals.h"
#include "pt2_mouse.h"
#include "pt2_audio.h"
#include "pt2_sampler.h"
#include "pt2_textout.h"
#include "pt2_rcfilter.h"
#include "pt2_pat2smp.h"
#include "pt2_downsamplers2x.h"

bool intMusic(void); // pt_modplayer.c
void storeTempVariables(void); // pt_modplayer.c

void doPat2Smp(void)
{
	moduleSample_t *s;

	ui.pat2SmpDialogShown = false;

	if (editor.sampleZero)
	{
		statusNotSampleZero();
		return;
	}

	editor.dPat2SmpBuf = (double *)malloc((MAX_SAMPLE_LEN*2) * sizeof (double));
	if (editor.dPat2SmpBuf == NULL)
	{
		statusOutOfMemory();
		return;
	}

	const int8_t oldRow = editor.songPlaying ? 0 : song->currRow;

	editor.isSMPRendering = true; // this must be set before restartSong()
	storeTempVariables();
	restartSong();
	song->row = oldRow;
	song->currRow = song->row;

	editor.blockMarkFlag = false;
	pointerSetMode(POINTER_MODE_MSG2, NO_CARRY);
	setStatusMessage("RENDERING...", NO_CARRY);
	modSetTempo(song->currBPM, true);
	editor.pat2SmpPos = 0;

	double dTickSamples = audio.dSamplesPerTick;

	editor.smpRenderingDone = false;
	while (!editor.smpRenderingDone)
	{
		const int32_t tickSamples = (int32_t)dTickSamples;

		const bool ended = !intMusic() || !editor.songPlaying;
		outputAudio(NULL, tickSamples);

		dTickSamples -= tickSamples; // keep fractional part
		dTickSamples += audio.dSamplesPerTick;

		if (ended)
			editor.smpRenderingDone = true;

	}
	editor.isSMPRendering = false;
	resetSong();

	int32_t renderLength = editor.pat2SmpPos;

	s = &song->samples[editor.currSample];

	// set back old row
	song->currRow = song->row = oldRow;

	// downsample oversampled buffer, normalize and quantize to 8-bit

	downsample2xDouble(editor.dPat2SmpBuf, renderLength);
	renderLength /= 2;

	double dAmp = 1.0;
	const double dPeak = getDoublePeak(editor.dPat2SmpBuf, renderLength);
	if (dPeak > 0.0)
		dAmp = INT8_MAX / dPeak;

	double dVol = 64.0 * dPeak;
	if (dVol > 64.0)
		dVol = 64.0;

	int8_t *smpPtr = &song->sampleData[s->offset];
	for (int32_t i = 0; i < renderLength; i++)
	{
		const int32_t smp = (const int32_t)round(editor.dPat2SmpBuf[i] * dAmp);
		assert(smp >= -128 && smp <= 127); // shouldn't happen according to dAmp (but just in case)
		smpPtr[i] = (int8_t)smp;
	}

	free(editor.dPat2SmpBuf);
	
	// clear the rest of the sample (if not full)
	if (renderLength < MAX_SAMPLE_LEN)
		memset(&song->sampleData[s->offset+renderLength], 0, MAX_SAMPLE_LEN - renderLength);

	if (editor.pat2SmpHQ)
	{
		strcpy(s->text, "pat2smp(a-3 ftune:+4)");
		s->fineTune = 4;
	}
	else
	{
		strcpy(s->text, "pat2smp(e-3 ftune: 0)");
		s->fineTune = 0;
	}

	s->length = (uint16_t)renderLength;
	s->volume = (int8_t)round(dVol);
	s->loopStart = 0;
	s->loopLength = 2;

	editor.samplePos = 0;
	fixSampleBeep(s);
	updateCurrSample();

	pointerSetMode(POINTER_MODE_IDLE, DO_CARRY);
	displayMsg("ROWS RENDERED!");
	setMsgPointer();
}
