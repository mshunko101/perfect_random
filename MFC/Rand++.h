
// Rand++.h: главный файл заголовка для приложения PROJECT_NAME
//

#pragma once

#ifndef __AFXWIN_H__
	#error "включить pch.h до включения этого файла в PCH"
#endif

#include "resource.h"		// основные символы


// CRandApp:
// Сведения о реализации этого класса: Rand++.cpp
//

class CRandApp : public CWinApp
{
public:
	CRandApp();

// Переопределение
public:
	virtual BOOL InitInstance();

// Реализация

	DECLARE_MESSAGE_MAP()
};

extern CRandApp theApp;
