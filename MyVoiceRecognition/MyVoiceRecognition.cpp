// MyVoiceRecognition.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "MyVoiceRecognition.h"

MyVoiceRecognition::~MyVoiceRecognition(void)
{
	if (g_vrec) g_vrec->Release(), g_vrec=0;
	if (g_source) g_source->Release(), g_source=0; 
	g_session->Release();
}

MyVoiceRecognition::MyVoiceRecognition()//PXCSession *session
{
	g_vrec=0;
	g_source=0;

	//CreateInstance
	g_session = PXCSession::CreateInstance();
	if (!g_session) {
        MessageBoxW(0,L"Failed to create an SDK session",L"Voice Recognition",MB_ICONEXCLAMATION|MB_OK);
    }

	/* Create an Audio Source */
	g_source = g_session->CreateAudioSource();
	if (!g_source) {
		printf("Failed to create the audio source");
	}

	/* Set Audio Source */
	g_source->ScanDevices();
	PXCAudioSource::DeviceInfo dinfo;
	vector<std::wstring> deviceNames;
	for(int d = g_source->QueryDeviceNum() - 1; d >= 0; d--)
	{
		if(g_source->QueryDeviceInfo(d, &dinfo)<PXC_STATUS_NO_ERROR) break;
		std::wstring dname(dinfo.name);
		deviceNames.push_back(dname);
	}

	// display devices and require a selection
	int deviceNum = deviceNames.size();
	int selectedDeviceId = 0;
	wprintf_s(L"Device list:\n");
	for (int i = 0; i < deviceNum; i++)
	{
		wprintf_s(L"Device[%d]: %s\n",i,deviceNames[i].c_str());
	}
	if (deviceNum > 0)
	{
		Sleep(1000);
		while (true)
		{
			wprintf_s(L"\nPlease select a device [0]-[%d]\n", deviceNum-1);
			cin >> selectedDeviceId;
			if (selectedDeviceId > -1 && selectedDeviceId < deviceNum)
			{
				wprintf_s(L"You selected device: [%d]%s\n", selectedDeviceId, deviceNames[selectedDeviceId].c_str());
				break;
			}
		}
	}
	else
	{
		wprintf_s(L"No audio device founded!");
		//return -1;
	}

	//set the active device
	if(g_source->QueryDeviceInfo(selectedDeviceId, &dinfo)<PXC_STATUS_NO_ERROR) 
		throw "myVoiceRecog.g_source->QueryDeviceInfo(selectedDeviceId, &dinfo)<PXC_STATUS_NO_ERROR";
	g_source->SetDevice(&dinfo);

	//Create a speech recognition instance
	g_session->CreateImpl<PXCSpeechRecognition>(&g_vrec);

	//Configure the Module
	g_vrec->QueryProfile(0, &pinfo);
	g_vrec->SetProfile(&pinfo);

	//g_vrec is a PXCSpeechRecognition instance.
	string path = "./commands.csv";
	voiceCmds.loadCsvFile(path);
	if (voiceCmds.getWCommands().size() != voiceCmds.cmds.size())
	{
		printf("Commands setting error!");
		exit(-1);
	}
	SetVoiceCommands(voiceCmds.getWCommands());
	printf("%d commands loaded!\n", voiceCmds.cmds.size());
	//std::string str = boost::lexical_cast<string>(voiceCmds.cmds.size()) + " commands loaded!";

	// source is a PXCAudioSource instance
	g_source->SetVolume(0.2f);

}

bool MyVoiceRecognition::SetVoiceCommands(std::vector<std::wstring> &cmds) {
    
    pxcUID grammar = 1;
	pxcEnum lbl=0;
	pxcCHAR ** _cmds = new pxcCHAR *[cmds.size()];
    for (std::vector<std::wstring >::iterator itr=cmds.begin();itr!=cmds.end();itr++,lbl++) {
		_cmds[lbl] = (pxcCHAR *)itr->c_str();
    }

	pxcStatus sts;
	//buildthe grammer
	sts = g_vrec->BuildGrammarFromStringList(grammar, _cmds, NULL, cmds.size());
	if (sts < PXC_STATUS_NO_ERROR) return false;
	//set the active grammer.
	sts = g_vrec->SetGrammar(grammar);
	if (sts < PXC_STATUS_NO_ERROR) return false;

    return true;
}

void PXCAPI MyVoiceRecognition::OnRecognition(const PXCSpeechRecognition::RecognitionData *data)
{
	int label = 0;
	if (data->scores[0].label<0) 
	{
		cout << "data->scores[0].label < 0\n";
		if (data->scores[0].tags[0])
			cout << "data->scores[0].tags[0]\n";
	} 
	else 
	{
		label = data->scores[0].label;
		if (label < 0)
		{
			printf("Wrowng label: [%d]\n", label);
			return;
		}
		std::string command = voiceCmds.cmds[label].command;
		std::string speech = voiceCmds.cmds[label].speech;
		printf("Voice recognized: speach [%s] command [%s]\n", speech.c_str(), command.c_str());

		//语音合成
		if ("time" == command)
		{
			time_t t;
			tm* local;
			tm* gmt;
			char buf[128]= {0};
			t = time(NULL);
			local = localtime(&t);
			gmt = gmtime(&t);
			strftime(buf, 128, "Current time is %X", gmt);
			printf("%s", buf);
		}
		else if ("date" == command)
		{
			time_t t;
			tm* gmt;
			char buf[128]= {0};
			t = time(NULL);
			gmt = gmtime(&t);
			strftime(buf, 128, "Current date is %x", gmt);
			printf("%s", buf);
		}
		else
		{
			printf("Other Command\n");
		}
	}
}

void  PXCAPI MyVoiceRecognition::OnAlert(const PXCSpeechRecognition::AlertData *data)
{
	std::string alert;
	switch (data->label)
	{
	case PXCSpeechRecognition::AlertType::ALERT_SNR_LOW: alert = "SNR low!";
		break;
	case PXCSpeechRecognition::AlertType::ALERT_SPEECH_UNRECOGNIZABLE: alert = "Speech unrecognizable!";
		break;
	case PXCSpeechRecognition::AlertType::ALERT_VOLUME_HIGH: alert = "Volume too high!";
		break;
	case PXCSpeechRecognition::AlertType::ALERT_VOLUME_LOW: alert = "Volume too low!";
		break;
	default:
		alert = "Unknown!";
		break;
	}
	printf("Voice unrecognized: [%s]\n", alert.c_str());
}

int _tmain(int argc, char** argv)
{
	MyVoiceRecognition myVoiceRecog;
	//Start/Stop Speech Recognition
	myVoiceRecog.g_vrec->StartRec(NULL, &myVoiceRecog);

	/* Start Recognition */
	printf("Start Recognition");
	int idx = 0;
	while (true) {
		;
	}

	//Stop recognition
	myVoiceRecog.g_vrec->StopRec();
	
	return 0;
}
