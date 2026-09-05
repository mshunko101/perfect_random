
# MSHUNKO RNG — Документация кода

> Версия: усиленная (физическая энтропия + каскады)
> Дата: 05.09.2026
> Автор: MSHUNKO

---

## Содержание

1. [Обзор архитектуры](#обзор-архитектуры)
2. [Point — структура координат](#point--структура-координат)
3. [RotationCalculator — расчёт периода](#rotationcalculator--расчёт-периода)
4. [EntropySource — каскад 1](#entropysource--каскад-1)
5. [MeanCore — каскад 2](#meancore--каскад-2)
6. [FantasyCore — каскад 3](#fantasycore--каскад-3)
7. [RNG — основной класс](#rng--основной-класс)
8. [Поток данных](#поток-данных)
9. [Параметры и константы](#параметры-и-константы)
10. [Сборка и зависимости](#сборка-и-зависимости)

---

## Обзор архитектуры

Генератор состоит из четырёх модулей, связанных в линейный каскад:

```
Системная энтропия ──▶ EntropySource ──▶ MeanCore ──▶ FantasyCore ──▶ выход
                                                     │
                          RotationCalculator ────────▶│ (сброс по периоду)
```

- **EntropySource** — читает физическую энтропию из ОС, возвращает `unsigned int`.
- **MeanCore** — нормирует в `[0, 1]`, добавляет шум MT19937.
- **FantasyCore** — коллизионное перемешивание через битовые размерности.
- **RotationCalculator** — вычисляет период сброса по физическим координатам.
- **RNG** — объединяет всё, управляет историей, коллизиями и сбросом.

---

## Point — структура координат

Трёхмерная точка. Используется в `RotationCalculator` для задания положений объектов.

| Поле | Тип | Описание |
|---|---|---|
| `x` | `double` | Координата X |
| `y` | `double` | Координата Y |
| `z` | `double` | Координата Z |

### Конструктор

```cpp
Point(double x = 0, double y = 0, double z = 0)
```

Создаёт точку с заданными координатами. По умолчанию — начало координат `(0, 0, 0)`.

---

## RotationCalculator — расчёт периода

Вычисляет период вращения по трём точкам в 3D-пространстве. Результат используется для определения момента сброса генератора.

### Конструкторы

```cpp
RotationCalculator(const Point& a, const Point& b, const Point& o, double t)
RotationCalculator(double time)
```

- Первый — произвольные точки и время.
- Второй — hardcoded координаты (значения по умолчанию из проекта), принимает только `time_AB`.

**Hardcoded координаты:**

| Точка | x | y | z |
|---|---|---|---|
| A | -3.0 | -0.5 | -2.5 |
| B | -5/3 | -5/6 | 25/4 |
| O_okr | -2.64 | -7.91 | 1.65 |

### Закрытые методы

| Метод | Сигнатура | Описание |
|---|---|---|
| `vectorBetween` | `Point(p1, p2)` | Вектор из p2 в p1: `(p1 - p2)` |
| `dotProduct` | `double(v1, v2)` | Скалярное произведение |
| `vectorLength` | `double(v)` | Длина вектора |
| `calculateOA` | `Point()` | Вектор OA = A - O_okr |
| `calculateOB` | `Point()` | Вектор OB = B - O_okr |
| `calculateAngle` | `double()` | Угол между OA и OB через `arccos(cos)` |
| `calculateOmega` | `double()` | Угловая скорость: `angle / time_AB` |

### Открытые методы

| Метод | Возвращает | Описание |
|---|---|---|
| `getPeriod()` | `double` | Период вращения: `2π / ω` (в годах) |
| `showResults()` | `void` | Выводит в консоль: длины векторов, угол, скорость, период |

### Формула

```
cos(θ) = (OA · OB) / (|OA| · |OB|)
θ = arccos(cos(θ))
ω = θ / time_AB
T = 2π / ω
```

---

## EntropySource — каскад 1

Кроссплатформенный источник системной энтропии. Заменяет оригинальный LCG (`AssociativityCore`).

### Платформы

| ОС | Метод | Заголовок |
|---|---|---|
| Windows Vista+ | `BCryptGenRandom` | `<bcrypt.h>` |
| Linux 3.17+ | `getrandom()` | `<sys/random.h>` |
| macOS 10.12+ | `getrandom()` | `<sys/random.h>` |
| Прочие Unix | `/dev/urandom` | `<fstream>` |

### Конструкторы

```cpp
EntropySource()
EntropySource(unsigned int /*seed*/)
```

Второй конструктор принимает `seed` для обратной совместимости, но **игнорирует** его — энтропия всегда берётся из системы.

### Методы

| Метод | Возвращает | Описание |
|---|---|---|
| `generate()` | `unsigned int` | Читает 4 байта системной энтропии, пропускает через `aval32()` |
| `reseed()` | `void` | Заглушка для совместимости (не нужна — каждый `generate()` читает свежую энтропию) |

### aval32 — avalanche-смеситель

```cpp
static uint32_t aval32(uint32_t x)
```

Три раунда `XOR-shift + умножение` на константу `0x45d9f3b`. Каждый входной бит влияет на ~50% выходных бит после первого раунда, на все 32 — после третьего. Убирает возможное смещение младших бит АЦП или системного источника.

---

## MeanCore — каскад 2

Нормирует выход `EntropySource` в диапазон `[0, 1]` и добавляет шум от MT19937.

### Конструктор

```cpp
MeanCore(double m = 1.0)
```

| Параметр | По умолчанию | Описание |
|---|---|---|
| `m` (mean) | 1.0 | Масштабирующий множитель для нормированного значения |

Внутри инициализирует:
- `dist` — `uniform_real_distribution<>(0.0, 1.0)`
- `gen` — `mt19937` с seed из `time(0)`

### Методы

| Метод | Возвращает | Описание |
|---|---|---|
| `adjust(base)` | `double` | Формула: `(base / UINT_MAX) * mean + dist(gen) * 0.1` |

### Что делает

1. Делит `base` на `UINT_MAX` → нормирует в `[0, 1]`.
2. Умножает на `mean` → масштабирует.
3. Добавляет шум MT19937 с амплитудой `0.1` → размывает возможные смещения источника.

---

## FantasyCore — каскад 3

Коллизионное перемешивание. При коллизии (повторе значения) извлекает битовые различия и использует их как поправки.

### Конструктор

```cpp
FantasyCore()
```

Инициализирует `mt19937` с seed из `time(0)`. `dimensions` — пустой вектор.

### Поля

| Поле | Тип | Описание |
|---|---|---|
| `dimensions` | `vector<vector<int>>` | Набор пар поправок `{0/1, 1/0}` |
| `gen` | `mt19937` | Генератор для случайного выбора знака поправки |

### Методы

#### `add_collision(unsigned int a, unsigned int b)`

Регистрирует коллизию между двумя значениями.

**Алгоритм:**
1. Если `a == b` — выход (идентичные значения не дают информации).
2. Оба числа представляются как `bitset<32>`.
3. Для каждого бита `i` от 0 до 31:
   - Если биты различаются — создаётся пара `{bits_a[i], bits_b[i]}`, инвертированная: если `a[i]=1, b[i]=0` → `{1, 0}`.
   - Проверяется, нет ли уже такой пары в `dimensions` (дубликаты не добавляются).
4. Новая пара добавляется в `dimensions`.

**Результат:** `dimensions` растёт с каждой новой коллизией. Чем больше коллизий — тем сильнее перемешивание.

#### `apply_fantasy(double base)`

Применяет накопленные размерности к значению.

**Алгоритм:**
1. Для каждой размерности в `dimensions`:
   - Случайно (через MT19937, `uniform_int_distribution<>(0,1)`) выбирает индекс 0 или 1.
   - Добавляет `dim[выбранный] * 0.01` к `base`.
2. Возвращает изменённый `base`.

**Эффект:** `base` сдвигается на сумму `±0.01` по каждой размерности. Знак каждой поправки случаен.

---

## RNG — основной класс

Объединяет все каскады, управляет историей, коллизиями и периодическим сбросом.

### Конструктор

```cpp
RNG(unsigned int seed, double period)
```

| Параметр | Описание |
|---|---|
| `seed` | Игнорируется (оставлен для совместимости). Энтропия берётся из системы. |
| `period` | Время прохождения от A до B в годах. Передаётся в `RotationCalculator`. |

**В конструкторе:**
1. Создаёт `EntropySource()` — без seed.
2. Создаёт `MeanCore()` и `FantasyCore()`.
3. Вычисляет `inc_max` через `RotationCalculator(period)`:
   ```
   inc_max = round(period_rotation * 365.25 * 24 * 3600 / 8)
   ```
   Где `period_rotation` — период вращения из `RotationCalculator`.
4. `inc_counter = 0`.

### Закрытые поля

| Поле | Тип | Описание |
|---|---|---|
| `entropyCore` | `EntropySource` | Каскад 1 — физическая энтропия |
| `meanCore` | `MeanCore` | Каскад 2 — нормализация + шум |
| `fantasyCore` | `FantasyCore` | Каскад 3 — коллизионное перемешивание |
| `history` | `unordered_set<unsigned int>` | Буфер выданных значений |
| `MAX_RETRIES` | `const int = 49` | Лимит перегенераций при коллизии |
| `MAX_HISTORY_SIZE` | `const int = 49` | Лимит размера истории |
| `inc_counter` | `size_t` | Счётчик выданных байт |
| `inc_max` | `size_t` | Порог сброса (в байтах) |

### Открытые методы

#### `get_period()`

```cpp
size_t get_period()
```

Возвращает `inc_max` — количество байт, после которого происходит сброс.

#### `generate(size_t size)`

```cpp
double generate(size_t size)
```

Основной метод генерации. Возвращает `double` в диапазоне, определённом каскадами.

**Алгоритм:**

```
1. Если inc_counter >= inc_max:
   - Пересоздать все ядра (EntropySource, MeanCore, FantasyCore)
   - inc_counter = 0

2. inc_counter += size

3. base = entropyCore.generate()

4. Цикл обработки коллизий (до MAX_RETRIES):
   - Если base уже в history:
     - previous_base = base
     - base = entropyCore.generate()
     - Если base == previous_base: handle_collision(previous, base)
     - retries++
   - Иначе: выход из цикла

5. Если retries >= MAX_RETRIES:
   - Пересоздать EntropySource
   - base = entropyCore.generate()
   - Очистить history и dimensions

6. Если history.size() >= MAX_HISTORY_SIZE:
   - Очистить history и dimensions

7. mean_adjusted = meanCore.adjust(base)
8. current = fantasyCore.apply_fantasy(mean_adjusted)
9. return current
```

#### `isCollision(unsigned int value)`

```cpp
bool isCollision(unsigned int value)
```

Проверяет, есть ли `value` в `history`. Если нет — добавляет. Возвращает `true` при коллизии.

#### `handle_collision(unsigned int a, unsigned int b)`

```cpp
void handle_collision(unsigned int a, unsigned int b)
```

Передаёт пару значений в `fantasyCore.add_collision(a, b)` для регистрации битовых размерностей.

#### `clearHistory()`

```cpp
void clearHistory()
```

Очищает `history`.

#### `getHistorySize()`

```cpp
size_t getHistorySize() const
```

Возвращает текущий размер `history`.

#### `reset()`

```cpp
void reset()
```

Полный сброс генератора:
1. Пересоздаёт `EntropySource`.
2. Очищает `history`.
3. Очищает `fantasyCore.dimensions`.

---

## Поток данных

```
                           generate(size)
                                │
                    ┌───────────▼───────────┐
                    │  inc_counter >= inc_max? │
                    └───────┬───────┬───────┘
                       да   │       │ нет
                    ┌───────▼───────┘
                    │  Пересоздать ядра      │
                    │  inc_counter = 0      │
                    └───────────┬───────────┘
                                │
                    ┌───────────▼───────────┐
                    │  base = entropyCore    │
                    │       .generate()      │
                    └───────────┬───────────┘
                                │
                    ┌───────────▼───────────┐
                    │  Коллизия в history?   │
                    └───┬───────────┬───────┘
                     да│           │нет
                    ┌───▼───┐     │
                    │regen +│     │
                    │collsn │     │
                    └───┬───┘     │
                        └────┬────┘
                             │
                    ┌────────▼────────┐
                    │ meanCore.adjust  │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │fantasyCore.apply│
                    └────────┬────────┘
                             │
                             ▼
                          выход (double)
```

---

## Параметры и константы

| Параметр | Значение | Где | Назначение |
|---|---|---|---|
| `MAX_RETRIES` | 49 | RNG | Лимит перегенераций при коллизии |
| `MAX_HISTORY_SIZE` | 49 | RNG | Лимит буфера истории |
| `mean` | 1.0 | MeanCore | Масштаб нормированного значения |
| Шум MT19937 | 0.1 | MeanCore | Амплитуда добавляемого шума |
| Поправка FantasyCore | 0.01 | FantasyCore | Шаг сдвига на одну размерность |
| Avalanche-константа | `0x45d9f3b` | EntropySource | Множитель смесителя aval32 |
| Делитель периода | 8 | RNG | Байт на один шаг (`inc_max`) |

### Почему 49?

Окно достаточно короткое, чтобы коллизии в пространстве `2^32` были редки, но достаточно длинное, чтобы `FantasyCore` накопил несколько размерностей для перемешивания. При переполнении — полная очистка.

---

## Сборка и зависимости

### Компиляция

```bash
# Linux / macOS
g++ -std=c++17 -O2 main.cpp -o rng_test

# Windows (MSVC)
cl /std:c++17 main.cpp
```

### Зависимости

- **Стандартные:** `<cmath>`, `<random>`, `<fstream>`, `<bitset>`, `<vector>`, `<unordered_set>`, `<cstdint>`, `<cstring>`
- **Windows:** `<windows.h>`, `<bcrypt.h>` (входит в Windows SDK)
- **Linux:** `<sys/random.h>` (ядро 3.17+)
- **macOS:** `<sys/random.h>` (10.12+)
- **Внешних библиотек нет**

### Проверка через Dieharder

```bash
# Генерация файла (10 МБ)
./rng_test

# Запуск всех тестов
dieharder -a -f rng_output.bin -v 1
```

---

## Макрос APPLICATION

Если определён `APPLICATION`, класс `RNG` наследуется от `RNGAbstract`:

```cpp
#ifdef APPLICATION
class RNG : public RNGAbstract {
#else
class RNG {
#endif
```

`RNGAbstract` должен предоставлять виртуальные методы `get_period()` и `generate(size_t)`. Без макроса `RNG` — самостоятельный класс без базового.

