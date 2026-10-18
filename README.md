# HW1: расширение Third Person Template

Домашнее задание 1 по Unreal Engine. Автор: Колесник Сергей Александрович, БПИ244.

Проект на **Unreal Engine 5.8.3** (C++, Windows), основанный на шаблоне Third Person. Название проекта: `HW1`. Шаблонный персонаж расширен спринтом, взаимодействием через делегат, отдельным состоянием анимации Sprint и механикой выносливости (Stamina).

## Управление

| Клавиша | Действие |
|---|---|
| W / A / S / D | Перемещение |
| Мышь | Поворот камеры |
| Space | Прыжок |
| Left Shift (удерживать) | Спринт |
| E | Взаимодействие: на экране появляется `Interaction triggered!` |
| Esc | Завершить Play In Editor |

## Описание выполненной работы

### 1. Наследование
- Создан C++ класс `AHomeworkCharacter` (`Source/HW1/HomeworkCharacter.h` / `.cpp`). Он наследуется от шаблонного `AHW1Character`, который, в свою очередь, наследуется от `ACharacter`, поэтому камера со SpringArm, перемещение, обзор и прыжок остаются в базовом классе и продолжают работать.
- Родительский класс `BP_ThirdPersonCharacter` изменён на `AHomeworkCharacter`.

### 2. Enhanced Input
- Добавлены два новых `UInputAction`: `IA_Sprint` (Left Shift) и `IA_Interact` (E), файлы в `Content/Input/Actions/`.
- Оба действия добавлены в `UInputMappingContext` шаблона `IMC_Default`.
- В `AHomeworkCharacter` созданы функции `StartSprint()`, `StopSprint()` и `Interact()`. Они привязаны через `BindAction` в `SetupPlayerInputComponent`:
  - `IA_Sprint`: `Started` -> `StartSprint`, `Completed` и `Canceled` -> `StopSprint`;
  - `IA_Interact`: `Started` -> `Interact`.
- Ассеты `IA_Sprint` и `IA_Interact` назначены в полях `SprintAction` и `InteractAction` у `BP_ThirdPersonCharacter`.

### 3. Делегаты и логика
- **Взаимодействие.** Объявлен `DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHomeworkInteractionSignature)` и свойство `OnInteraction` (`BlueprintAssignable`). При нажатии E функция `Interact()` вызывает `OnInteraction.Broadcast()`. В `BP_ThirdPersonCharacter` на этот делегат подписан Custom Event, который в `BeginPlay` выводит `Interaction triggered!` через Print String.
- **Спринт.** При зажатии Shift `MaxWalkSpeed` меняется с `WalkSpeed` (500) на `SprintSpeed` (900), при отпускании возвращается к 500. Состояние хранится в булевой переменной `bIsSprinting` (`UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)`). Скорости настраиваются в Blueprint (`EditDefaultsOnly`).

### 4. Анимации (State Machine)
- Создан C++ класс `UHomeworkAnimInstance` (`HomeworkAnimInstance.h` / `.cpp`), который является родительским классом стандартного `ABP_Unarmed`. В `NativeUpdateAnimation` он читает `bIsSprinting` из `AHomeworkCharacter` и отдаёт его в Animation Blueprint (`BlueprintReadOnly`).
- В State Machine `Locomotion` добавлено состояние `Sprint` со стандартной анимацией бега.
- Правила переходов: обычное движение -> `Sprint` при `bIsSprinting == true` и обратно при `bIsSprinting == false`.

### 5. Выносливость (Stamina), дополнительное условие
Реализована на C++ в `AHomeworkCharacter::Tick`.

| Параметр | Значение по умолчанию | Назначение |
|---|---|---|
| `MaxStamina` | 100 | максимум выносливости |
| `StaminaDrainRate` | 25 в секунду | расход во время спринта |
| `StaminaRegenRate` | 20 в секунду | восстановление вне спринта |
| `StaminaRegenDelay` | 0,8 с | пауза после спринта перед восстановлением |
| `MinimumSprintStamina` | 20 | минимум выносливости для начала спринта |

- Во время спринта выносливость уменьшается, в остальное время восстанавливается (после небольшой задержки).
- Когда выносливость доходит до 0, спринт принудительно прекращается, даже если Shift удерживается. После истощения нужно отпустить Shift, автоматически спринт не возобновится. Начать спринт с нулевой выносливостью (или ниже `MinimumSprintStamina`) нельзя.
- Значение выводится на экран через HUD на UMG: `Content/ThirdPerson/UI/WBP_StaminaHUD` показывает число и шкалу выносливости (данные обновляются Blueprint-привязками, для этого есть `GetStaminaFraction()`). Виджет добавляет локальный `BP_UMGPlayerController`, а на стартовом уровне выбран `BP_UMGGameMode`.

## Структура проекта

| Путь | Содержимое |
|---|---|
| `Source/HW1/HomeworkCharacter.h/.cpp` | персонаж: ввод, спринт, делегат, выносливость |
| `Source/HW1/HomeworkAnimInstance.h/.cpp` | передача `bIsSprinting` в Animation Blueprint |
| `Content/Input/Actions/IA_Sprint`, `IA_Interact` | новые Input Actions |
| `Content/Input/IMC_Default` | Input Mapping Context с новыми действиями |
| `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter` | Blueprint персонажа (подписка на делегат) |
| `Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed` | Animation Blueprint с состоянием `Sprint` |
| `Content/ThirdPerson/UI/WBP_StaminaHUD` | HUD выносливости |

## Запуск
1. Открыть `HW1.uproject` в Unreal Engine 5.8.3 и при запросе собрать C++ модуль (либо сгенерировать файлы проекта и собрать в IDE).
2. Запустить стартовый уровень через Play In Editor.

## Git
Используется стандартный `.gitignore` для Unreal Engine: папки `Binaries`, `DerivedDataCache`, `Intermediate` и `Saved` в репозиторий не загружаются.
