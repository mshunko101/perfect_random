// pch.h: это предварительно скомпилированный заголовочный файл.
// Перечисленные ниже файлы компилируются только один раз, что ускоряет последующие сборки.
// Это также влияет на работу IntelliSense, включая многие функции просмотра и завершения кода.
// Однако изменение любого из приведенных здесь файлов между операциями сборки приведет к повторной компиляции всех(!) этих файлов.
// Не добавляйте сюда файлы, которые планируете часто изменять, так как в этом случае выигрыша в производительности не будет.
#pragma once
#ifndef PCH_H
#define PCH_H

// Добавьте сюда заголовочные файлы для предварительной компиляции
#include "framework.h"

#include "randpp_pure_c.h"

class  RNGAbstract
{
public:
	virtual double generate(size_t size) = 0;
};



class  RandPP_PureC : public RNGAbstract
{
public:
	RandPP_PureC()
	{
		init_rng((unsigned int)time(NULL));
	}
	virtual double generate(size_t size) override
	{
		return generate_random(size);
	}
};

#endif //PCH_H
