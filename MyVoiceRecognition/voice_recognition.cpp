/*******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

*******************************************************************************/
#include <Windows.h>
#include <CommCtrl.h>
#include <vector>
#include "resource.h"
#include "pxcsensemanager.h"
#include "pxcspeechrecognition.h"
#include "voice_recognition.h"
#include "main.h"

/* New API */
 bool  SetVoiceCommands(std::vector<std::wstring> &cmds);
 bool  SetGrammarFromFile(HWND hWnd, pxcCHAR* GrammarFilename);
 bool  GetGrammarCompileErrors(pxcCHAR** grammarCompileErrors);
 bool  SetVocabularyFromFile(pxcCHAR* VocFilename);

class MyHandler: public PXCSpeechRecognition::Handler {
public:

    MyHandler(HWND hWnd):m_hWnd(hWnd) {}
	virtual ~MyHandler(){}

	virtual void PXCAPI OnRecognition(const PXCSpeechRecognition::RecognitionData *data) {
		if (data->scores[0].label<0) {
			PrintConsole(m_hWnd,(pxcCHAR*)data->scores[0].sentence);
			if (data->scores[0].tags[0])
				PrintConsole(m_hWnd,(pxcCHAR*)data->scores[0].tags);
		} else {
			ClearScores(m_hWnd);
			for (int i=0;i<sizeof(data->scores)/sizeof(data->scores[0]);i++) {
				if (data->scores[i].label < 0 || data->scores[i].confidence == 0) continue;
				SetScore(m_hWnd,data->scores[i].label,data->scores[i].confidence);
			}
			if (data->scores[0].tags[0])
				PrintStatus(m_hWnd,(pxcCHAR*)data->scores[0].tags);
		}
	}

	virtual void PXCAPI OnAlert(const PXCSpeechRecognition::AlertData *data) {
		if(data->label == PXCSpeechRecognition::AlertType::ALERT_SPEECH_UNRECOGNIZABLE)
		{
			ClearScores(m_hWnd);
		}
		PrintStatus(m_hWnd,AlertToString(data->label));
	}

protected:

	HWND m_hWnd;
};

static volatile bool g_stop=false;
static volatile bool g_stopped=true;
static PXCSpeechRecognition* g_vrec=0;
static PXCAudioSource* g_source=0;
static MyHandler* g_handler=0;

static void CleanUp(void) {
	if (g_handler) delete g_handler, g_handler=0;
	if (g_vrec) g_vrec->Release(), g_vrec=0;
	if (g_source) g_source->Release(), g_source=0; 
	g_stopped=true;
}

DWORD WINAPI RecThread(HWND hWnd) {
	g_stopped=false;

	/* Create an Audio Source */
	g_source=g_session->CreateAudioSource();
	if (!g_source) {
		PrintStatus(hWnd,L"Failed to create the audio source");
		CleanUp();
		return 0;
	}

	/* Set Audio Source */
	PXCAudioSource::DeviceInfo dinfo=GetAudioSource(hWnd);
	pxcStatus sts=g_source->SetDevice(&dinfo);
	if (sts<PXC_STATUS_NO_ERROR) {
		PrintStatus(hWnd,L"Failed to set the audio source");
		CleanUp();
		return 0;
	}

	/* Set Module */
	PXCSession::ImplDesc mdesc={};
	mdesc.iuid=GetModule(hWnd);
	sts=g_session->CreateImpl<PXCSpeechRecognition>(&g_vrec);
	if (sts<PXC_STATUS_NO_ERROR) {
		PrintStatus(hWnd,L"Failed to create the module instance");
		CleanUp();
		return 0;
	}

	/* Set configuration according to user-selected language */
	PXCSpeechRecognition::ProfileInfo pinfo;
	int language_idx = GetLanguage(hWnd);
	g_vrec->QueryProfile(language_idx, &pinfo);
	sts = g_vrec->SetProfile(&pinfo);
	if (sts < PXC_STATUS_NO_ERROR)
	{
		PrintStatus(hWnd,L"Failed to set language");
		CleanUp();
		return 0;
	}

		/* Set Command/Control or Dictation */
		if (IsCommandControl(hWnd)) {
			// Connamd/Control mode
			std::vector<std::wstring> cmds=GetCommands(hWnd);

			if (g_file && g_file[0] != '\0') {
				wchar_t *fileext1 = wcsrchr(g_file, L'.');
				const wchar_t *fileext = ++fileext1;
				if (wcscmp(fileext,L"list") == 0 || wcscmp(fileext,L"txt") == 0) {
					// voice commands loaded from file: put into gui
					FillCommandListConsole(hWnd,g_file);
					cmds=GetCommands(hWnd);
					if (cmds.empty())
						PrintStatus(hWnd,L"Command List Load Errors");
				}

				// input Command/Control grammar file available, use it
				if (!SetGrammarFromFile(hWnd, g_file)) {
					PrintStatus(hWnd,L"Can not set Grammar From File.");
					CleanUp();
					return -1;
				};

			} else if (!cmds.empty()) {
				// voice commands available, use them
				if (!SetVoiceCommands(cmds)) {
					PrintStatus(hWnd,L"Can not set Command List.");
					CleanUp();
					return -1;
				};

			} else {
				// revert to dictation mode
				PrintStatus(hWnd,L"No Command List or Grammar. Using dictation instead.");
				if (v_file && v_file[0] != '\0')  SetVocabularyFromFile(v_file);
				g_vrec->SetDictation();
			}

		} else {
			// Dictation mode
			if (v_file && v_file[0] != '\0')  SetVocabularyFromFile(v_file);
			g_vrec->SetDictation();
		}

	/* Start Recognition */
	PrintStatus(hWnd,L"Start Recognition");
	g_handler=new MyHandler(hWnd);
	sts=g_vrec->StartRec(g_source, g_handler);
	if (sts<PXC_STATUS_NO_ERROR) {
		PrintStatus(hWnd,L"Failed to start recognition");
		CleanUp();
		return 0;
	}

	PrintStatus(hWnd,L"Recognition Started");
	return 0;
}

void Start(HWND hwndDlg) {
	if (!g_stopped) return;
	g_stop=false;
	RecThread(hwndDlg);
	Sleep(5);
}

bool Stop(HWND hwndDlg) {

	if (g_vrec) g_vrec->StopRec();
	g_stopped=true;
	CleanUp();
	PrintStatus(hwndDlg,L"Recognition Finished");
	return g_stopped;
}

bool IsRunning(void) {
	return !g_stopped;
}

bool SetVoiceCommands(std::vector<std::wstring> &cmds) {
    
    pxcUID grammar = 1;
	pxcEnum lbl=0;
	pxcCHAR ** _cmds = new pxcCHAR *[cmds.size()];
    for (std::vector<std::wstring >::iterator itr=cmds.begin();itr!=cmds.end();itr++,lbl++) {
		_cmds[lbl] = (pxcCHAR *)itr->c_str();
    }

	pxcStatus sts;

	sts = g_vrec->BuildGrammarFromStringList(grammar,_cmds,NULL,cmds.size());
	if (sts < PXC_STATUS_NO_ERROR) return false;

    sts = g_vrec->SetGrammar(grammar);
	if (sts < PXC_STATUS_NO_ERROR) return false;

    return true;
}

bool SetGrammarFromFile(HWND hWnd, pxcCHAR* GrammarFilename) {
    if (!g_vrec) {
        return true; /// not setting stackable profile now
    }           
 
    pxcUID grammar = 1;

	pxcStatus sts = g_vrec->BuildGrammarFromFile(grammar, PXCSpeechRecognition::GFT_NONE, GrammarFilename);
	if (sts < PXC_STATUS_NO_ERROR) {
		PrintStatus(hWnd,L"Grammar Compile Errors:");
		PrintStatus(hWnd,(pxcCHAR *) g_vrec->GetGrammarCompileErrors(grammar)); //SM Is itapplicable?
		return false;
	}

	sts = g_vrec->SetGrammar(grammar);
	if (sts < PXC_STATUS_NO_ERROR) return false;


    return true;
}

bool SetVocabularyFromFile(pxcCHAR* VocFilename) {
    if (!g_vrec) {
        return true; /// not setting stackable profile now
    }
 
	pxcStatus sts = g_vrec->AddVocabToDictation(PXCSpeechRecognition::VFT_LIST, VocFilename);
	if (sts < PXC_STATUS_NO_ERROR) return false;

    return true;
}
