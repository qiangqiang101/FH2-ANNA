// ANNA's text in the game's language.
//
// The language: the string tables' manager ([game+0x3662990]) has the text language's code, an
// MSVC std::string ("EN", "DE", "CHT"...), at +0x30 (as Extra Options reads it). Its names for
// the game's places and modes are the game's own (PauseMenu.str, Map.str, BucketList.str...);
// Czech and Hungarian stay English, as the game's own text is there (see english_there).
#include "lang.h"

#include <atomic>
#include <cstring>
#include <string>

namespace fh2anna {
namespace {

constexpr uintptr_t RVA_STRINGS = 0x3662990;

// The languages, in the table's order (English is the key itself; GB is English).
const char* const kCodes[] = {"BR", "DE", "FR", "ES", "MX", "IT", "JP", "KO", "CHT", "CZ", "HU", "PL", "RU", "NL"};
constexpr int kLangs       = sizeof kCodes / sizeof kCodes[0];

struct Text {
    const char* en;
    const char* t[kLangs];
};

// clang-format off
const Text kTexts[] = {
{"Car", {"Carro", "Auto", "Voiture", "Coche", "Auto", "Auto", "車", "자동차", "車輛", "Auto", "Autó", "Samochód", "Машина", "Auto"}},
{"Lights", {"Luzes", "Lichter", "Feux", "Luces", "Luces", "Luci", "ライト", "라이트", "車燈", "Světla", "Lámpák", "Światła", "Свет", "Lichten"}},
{"Doors", {"Portas", "Türen", "Portières", "Puertas", "Puertas", "Portiere", "ドア", "도어", "車門", "Dveře", "Ajtók", "Drzwi", "Двери", "Portieren"}},
{"Tune Car", {"Tunar carro", "Auto tunen", "Régler voiture", "Tunear coche", "Tunear auto", "Elabora auto", "チューニング", "자동차 튜닝", "調校車輛", "Vyladit auto", "Autó hangolása", "Tuninguj samochód", "Тюнинговать машину", "Auto afstellen"}},
{"Enable Auto Drive", {"Ativar direção automática", "Autopilot aktivieren", "Activer la conduite auto", "Activar conducción automática", "Activar manejo automático", "Attiva guida automatica", "自動運転を有効にする", "자동 주행 켜기", "啟用自動駕駛", "Zapnout autopilota", "Robotpilóta bekapcsolása", "Włącz autopilota", "Включить автопилот", "Automatisch rijden aan"}},
{"Disable Auto Drive", {"Desativar direção automática", "Autopilot deaktivieren", "Désactiver la conduite auto", "Desactivar conducción automática", "Desactivar manejo automático", "Disattiva guida automatica", "自動運転を無効にする", "자동 주행 끄기", "停用自動駕駛", "Vypnout autopilota", "Robotpilóta kikapcsolása", "Wyłącz autopilota", "Выключить автопилот", "Automatisch rijden uit"}},
{"Headlights On", {"Ligar faróis", "Scheinwerfer an", "Allumer les phares", "Encender faros", "Encender faros", "Accendi fari", "ヘッドライト オン", "전조등 켜기", "開啟頭燈", "Zapnout světlomety", "Fényszórók be", "Włącz reflektory", "Включить фары", "Koplampen aan"}},
{"Headlights Off", {"Desligar faróis", "Scheinwerfer aus", "Éteindre les phares", "Apagar faros", "Apagar faros", "Spegni fari", "ヘッドライト オフ", "전조등 끄기", "關閉頭燈", "Vypnout světlomety", "Fényszórók ki", "Wyłącz reflektory", "Выключить фары", "Koplampen uit"}},
{"Hazard Lights On", {"Ligar pisca-alerta", "Warnblinker an", "Allumer les feux de détresse", "Encender luces de emergencia", "Encender intermitentes", "Accendi luci di emergenza", "ハザードランプ オン", "비상등 켜기", "開啟警示燈", "Zapnout výstražná světla", "Vészvillogó be", "Włącz światła awaryjne", "Включить аварийку", "Alarmlichten aan"}},
{"Hazard Lights Off", {"Desligar pisca-alerta", "Warnblinker aus", "Éteindre les feux de détresse", "Apagar luces de emergencia", "Apagar intermitentes", "Spegni luci di emergenza", "ハザードランプ オフ", "비상등 끄기", "關閉警示燈", "Vypnout výstražná světla", "Vészvillogó ki", "Wyłącz światła awaryjne", "Выключить аварийку", "Alarmlichten uit"}},
{"Left Indicator On", {"Ligar seta esquerda", "Blinker links an", "Allumer le clignotant gauche", "Encender intermitente izquierdo", "Encender direccional izquierda", "Accendi freccia sinistra", "左ウインカー オン", "왼쪽 방향 지시등 켜기", "開啟左方向燈", "Zapnout levý blinkr", "Bal index be", "Włącz lewy kierunkowskaz", "Включить левый поворотник", "Richtingaanwijzer links aan"}},
{"Left Indicator Off", {"Desligar seta esquerda", "Blinker links aus", "Éteindre le clignotant gauche", "Apagar intermitente izquierdo", "Apagar direccional izquierda", "Spegni freccia sinistra", "左ウインカー オフ", "왼쪽 방향 지시등 끄기", "關閉左方向燈", "Vypnout levý blinkr", "Bal index ki", "Wyłącz lewy kierunkowskaz", "Выключить левый поворотник", "Richtingaanwijzer links uit"}},
{"Right Indicator On", {"Ligar seta direita", "Blinker rechts an", "Allumer le clignotant droit", "Encender intermitente derecho", "Encender direccional derecha", "Accendi freccia destra", "右ウインカー オン", "오른쪽 방향 지시등 켜기", "開啟右方向燈", "Zapnout pravý blinkr", "Jobb index be", "Włącz prawy kierunkowskaz", "Включить правый поворотник", "Richtingaanwijzer rechts aan"}},
{"Right Indicator Off", {"Desligar seta direita", "Blinker rechts aus", "Éteindre le clignotant droit", "Apagar intermitente derecho", "Apagar direccional derecha", "Spegni freccia destra", "右ウインカー オフ", "오른쪽 방향 지시등 끄기", "關閉右方向燈", "Vypnout pravý blinkr", "Jobb index ki", "Wyłącz prawy kierunkowskaz", "Выключить правый поворотник", "Richtingaanwijzer rechts uit"}},
{"Open All", {"Abrir tudo", "Alle öffnen", "Tout ouvrir", "Abrir todo", "Abrir todo", "Apri tutto", "すべて開ける", "모두 열기", "全部打開", "Otevřít vše", "Összes nyitása", "Otwórz wszystko", "Открыть всё", "Alles openen"}},
{"Close All", {"Fechar tudo", "Alle schließen", "Tout fermer", "Cerrar todo", "Cerrar todo", "Chiudi tutto", "すべて閉める", "모두 닫기", "全部關閉", "Zavřít vše", "Összes zárása", "Zamknij wszystko", "Закрыть всё", "Alles sluiten"}},
{"Open Roof", {"Abrir teto", "Dach öffnen", "Ouvrir le toit", "Abrir techo", "Abrir techo", "Apri tetto", "ルーフを開ける", "지붕 열기", "打開車頂", "Otevřít střechu", "Tető nyitása", "Otwórz dach", "Открыть крышу", "Dak openen"}},
{"Close Roof", {"Fechar teto", "Dach schließen", "Fermer le toit", "Cerrar techo", "Cerrar techo", "Chiudi tetto", "ルーフを閉める", "지붕 닫기", "關閉車頂", "Zavřít střechu", "Tető zárása", "Zamknij dach", "Закрыть крышу", "Dak sluiten"}},
{"Open Hood", {"Abrir capô", "Motorhaube öffnen", "Ouvrir le capot", "Abrir capó", "Abrir cofre", "Apri cofano", "ボンネットを開ける", "후드 열기", "打開引擎蓋", "Otevřít kapotu", "Motorháztető nyitása", "Otwórz maskę", "Открыть капот", "Motorkap openen"}},
{"Close Hood", {"Fechar capô", "Motorhaube schließen", "Fermer le capot", "Cerrar capó", "Cerrar cofre", "Chiudi cofano", "ボンネットを閉める", "후드 닫기", "關閉引擎蓋", "Zavřít kapotu", "Motorháztető zárása", "Zamknij maskę", "Закрыть капот", "Motorkap sluiten"}},
{"Open Trunk", {"Abrir porta-malas", "Kofferraum öffnen", "Ouvrir le coffre", "Abrir maletero", "Abrir cajuela", "Apri bagagliaio", "トランクを開ける", "트렁크 열기", "打開後車箱", "Otevřít kufr", "Csomagtartó nyitása", "Otwórz bagażnik", "Открыть багажник", "Kofferbak openen"}},
{"Close Trunk", {"Fechar porta-malas", "Kofferraum schließen", "Fermer le coffre", "Cerrar maletero", "Cerrar cajuela", "Chiudi bagagliaio", "トランクを閉める", "트렁크 닫기", "關閉後車箱", "Zavřít kufr", "Csomagtartó zárása", "Zamknij bagażnik", "Закрыть багажник", "Kofferbak sluiten"}},
{"Open Front Left Door", {"Abrir porta dianteira esquerda", "Tür vorne links öffnen", "Ouvrir la portière avant gauche", "Abrir puerta delantera izquierda", "Abrir puerta delantera izquierda", "Apri portiera anteriore sinistra", "左フロントドアを開ける", "왼쪽 앞문 열기", "打開左前車門", "Otevřít levé přední dveře", "Bal első ajtó nyitása", "Otwórz lewe przednie drzwi", "Открыть переднюю левую дверь", "Portier linksvoor openen"}},
{"Close Front Left Door", {"Fechar porta dianteira esquerda", "Tür vorne links schließen", "Fermer la portière avant gauche", "Cerrar puerta delantera izquierda", "Cerrar puerta delantera izquierda", "Chiudi portiera anteriore sinistra", "左フロントドアを閉める", "왼쪽 앞문 닫기", "關閉左前車門", "Zavřít levé přední dveře", "Bal első ajtó zárása", "Zamknij lewe przednie drzwi", "Закрыть переднюю левую дверь", "Portier linksvoor sluiten"}},
{"Open Front Right Door", {"Abrir porta dianteira direita", "Tür vorne rechts öffnen", "Ouvrir la portière avant droite", "Abrir puerta delantera derecha", "Abrir puerta delantera derecha", "Apri portiera anteriore destra", "右フロントドアを開ける", "오른쪽 앞문 열기", "打開右前車門", "Otevřít pravé přední dveře", "Jobb első ajtó nyitása", "Otwórz prawe przednie drzwi", "Открыть переднюю правую дверь", "Portier rechtsvoor openen"}},
{"Close Front Right Door", {"Fechar porta dianteira direita", "Tür vorne rechts schließen", "Fermer la portière avant droite", "Cerrar puerta delantera derecha", "Cerrar puerta delantera derecha", "Chiudi portiera anteriore destra", "右フロントドアを閉める", "오른쪽 앞문 닫기", "關閉右前車門", "Zavřít pravé přední dveře", "Jobb első ajtó zárása", "Zamknij prawe przednie drzwi", "Закрыть переднюю правую дверь", "Portier rechtsvoor sluiten"}},
{"Open Rear Left Door", {"Abrir porta traseira esquerda", "Tür hinten links öffnen", "Ouvrir la portière arrière gauche", "Abrir puerta trasera izquierda", "Abrir puerta trasera izquierda", "Apri portiera posteriore sinistra", "左リアドアを開ける", "왼쪽 뒷문 열기", "打開左後車門", "Otevřít levé zadní dveře", "Bal hátsó ajtó nyitása", "Otwórz lewe tylne drzwi", "Открыть заднюю левую дверь", "Portier linksachter openen"}},
{"Close Rear Left Door", {"Fechar porta traseira esquerda", "Tür hinten links schließen", "Fermer la portière arrière gauche", "Cerrar puerta trasera izquierda", "Cerrar puerta trasera izquierda", "Chiudi portiera posteriore sinistra", "左リアドアを閉める", "왼쪽 뒷문 닫기", "關閉左後車門", "Zavřít levé zadní dveře", "Bal hátsó ajtó zárása", "Zamknij lewe tylne drzwi", "Закрыть заднюю левую дверь", "Portier linksachter sluiten"}},
{"Open Rear Right Door", {"Abrir porta traseira direita", "Tür hinten rechts öffnen", "Ouvrir la portière arrière droite", "Abrir puerta trasera derecha", "Abrir puerta trasera derecha", "Apri portiera posteriore destra", "右リアドアを開ける", "오른쪽 뒷문 열기", "打開右後車門", "Otevřít pravé zadní dveře", "Jobb hátsó ajtó nyitása", "Otwórz prawe tylne drzwi", "Открыть заднюю правую дверь", "Portier rechtsachter openen"}},
{"Close Rear Right Door", {"Fechar porta traseira direita", "Tür hinten rechts schließen", "Fermer la portière arrière droite", "Cerrar puerta trasera derecha", "Cerrar puerta trasera derecha", "Chiudi portiera posteriore destra", "右リアドアを閉める", "오른쪽 뒷문 닫기", "關閉右後車門", "Zavřít pravé zadní dveře", "Jobb hátsó ajtó zárása", "Zamknij prawe tylne drzwi", "Закрыть заднюю правую дверь", "Portier rechtsachter sluiten"}},
{"Open Storage", {"Abrir compartimento", "Staufach öffnen", "Ouvrir le rangement", "Abrir compartimento", "Abrir compartimento", "Apri vano", "収納を開ける", "수납공간 열기", "打開置物箱", "Otevřít úložný prostor", "Tárolórekesz nyitása", "Otwórz schowek", "Открыть отсек", "Opbergvak openen"}},
{"Close Storage", {"Fechar compartimento", "Staufach schließen", "Fermer le rangement", "Cerrar compartimento", "Cerrar compartimento", "Chiudi vano", "収納を閉める", "수납공간 닫기", "關閉置物箱", "Zavřít úložný prostor", "Tárolórekesz zárása", "Zamknij schowek", "Закрыть отсек", "Opbergvak sluiten"}},
{"Open Second Storage", {"Abrir segundo compartimento", "Zweites Staufach öffnen", "Ouvrir le second rangement", "Abrir segundo compartimento", "Abrir segundo compartimento", "Apri secondo vano", "2番目の収納を開ける", "두 번째 수납공간 열기", "打開第二置物箱", "Otevřít druhý úložný prostor", "Második tárolórekesz nyitása", "Otwórz drugi schowek", "Открыть второй отсек", "Tweede opbergvak openen"}},
{"Close Second Storage", {"Fechar segundo compartimento", "Zweites Staufach schließen", "Fermer le second rangement", "Cerrar segundo compartimento", "Cerrar segundo compartimento", "Chiudi secondo vano", "2番目の収納を閉める", "두 번째 수납공간 닫기", "關閉第二置物箱", "Zavřít druhý úložný prostor", "Második tárolórekesz zárása", "Zamknij drugi schowek", "Закрыть второй отсек", "Tweede opbergvak sluiten"}},
{"Open Pop-up Headlights", {"Levantar faróis escamoteáveis", "Klappscheinwerfer ausklappen", "Sortir les phares escamotables", "Subir faros escamoteables", "Levantar faros abatibles", "Alza fari a scomparsa", "リトラクタブルライトを上げる", "팝업 헤드라이트 올리기", "升起跳燈", "Vyklopit výklopné světlomety", "Bukólámpák felnyitása", "Podnieś chowane reflektory", "Поднять выдвижные фары", "Klapkoplampen uitklappen"}},
{"Close Pop-up Headlights", {"Baixar faróis escamoteáveis", "Klappscheinwerfer einklappen", "Rentrer les phares escamotables", "Bajar faros escamoteables", "Bajar faros abatibles", "Abbassa fari a scomparsa", "リトラクタブルライトを下げる", "팝업 헤드라이트 내리기", "收起跳燈", "Sklopit výklopné světlomety", "Bukólámpák lecsukása", "Opuść chowane reflektory", "Опустить выдвижные фары", "Klapkoplampen inklappen"}},
{"Open Left Pop-up Headlight", {"Levantar farol escamoteável esquerdo", "Linken Klappscheinwerfer ausklappen", "Sortir le phare escamotable gauche", "Subir faro escamoteable izquierdo", "Levantar faro abatible izquierdo", "Alza faro a scomparsa sinistro", "左リトラクタブルライトを上げる", "왼쪽 팝업 헤드라이트 올리기", "升起左跳燈", "Vyklopit levý výklopný světlomet", "Bal bukólámpa felnyitása", "Podnieś lewy chowany reflektor", "Поднять левую выдвижную фару", "Linker klapkoplamp uitklappen"}},
{"Close Left Pop-up Headlight", {"Baixar farol escamoteável esquerdo", "Linken Klappscheinwerfer einklappen", "Rentrer le phare escamotable gauche", "Bajar faro escamoteable izquierdo", "Bajar faro abatible izquierdo", "Abbassa faro a scomparsa sinistro", "左リトラクタブルライトを下げる", "왼쪽 팝업 헤드라이트 내리기", "收起左跳燈", "Sklopit levý výklopný světlomet", "Bal bukólámpa lecsukása", "Opuść lewy chowany reflektor", "Опустить левую выдвижную фару", "Linker klapkoplamp inklappen"}},
{"Open Right Pop-up Headlight", {"Levantar farol escamoteável direito", "Rechten Klappscheinwerfer ausklappen", "Sortir le phare escamotable droit", "Subir faro escamoteable derecho", "Levantar faro abatible derecho", "Alza faro a scomparsa destro", "右リトラクタブルライトを上げる", "오른쪽 팝업 헤드라이트 올리기", "升起右跳燈", "Vyklopit pravý výklopný světlomet", "Jobb bukólámpa felnyitása", "Podnieś prawy chowany reflektor", "Поднять правую выдвижную фару", "Rechter klapkoplamp uitklappen"}},
{"Close Right Pop-up Headlight", {"Baixar farol escamoteável direito", "Rechten Klappscheinwerfer einklappen", "Rentrer le phare escamotable droit", "Bajar faro escamoteable derecho", "Bajar faro abatible derecho", "Abbassa faro a scomparsa destro", "右リトラクタブルライトを下げる", "오른쪽 팝업 헤드라이트 내리기", "收起右跳燈", "Sklopit pravý výklopný světlomet", "Jobb bukólámpa lecsukása", "Opuść prawy chowany reflektor", "Опустить правую выдвижную фару", "Rechter klapkoplamp inklappen"}},
{"Open Spoiler", {"Abrir aerofólio", "Spoiler ausfahren", "Sortir l'aileron", "Subir alerón", "Subir alerón", "Alza alettone", "スポイラーを上げる", "스포일러 올리기", "升起尾翼", "Vysunout spoiler", "Légterelő kiengedése", "Wysuń spojler", "Поднять спойлер", "Spoiler uitklappen"}},
{"Close Spoiler", {"Fechar aerofólio", "Spoiler einfahren", "Rentrer l'aileron", "Bajar alerón", "Bajar alerón", "Abbassa alettone", "スポイラーを下げる", "스포일러 내리기", "收起尾翼", "Zasunout spoiler", "Légterelő behúzása", "Schowaj spojler", "Опустить спойлер", "Spoiler inklappen"}},
{"Set Route To Nearest", {"Traçar rota até o mais próximo", "Route zum nächsten Ziel", "Itinéraire vers le plus proche", "Ruta al más cercano", "Ruta al más cercano", "Percorso verso il più vicino", "最寄りへのルートを設定", "가장 가까운 곳으로 경로 설정", "設定前往最近的路線", "Trasa k nejbližšímu", "Útvonal a legközelebbihez", "Trasa do najbliższego", "Маршрут до ближайшего", "Route naar dichtstbijzijnde"}},
{"Race", {"Corrida", "Rennen", "Course", "Carrera", "Carrera", "Gara", "レース", "레이스", "比賽", "Závod", "Verseny", "Wyścig", "Гонка", "Race"}},
{"Bucket List", {"Lista de Experiências", "Fahrspaßliste", "Défi collector", "Experiencias vitales", "Lista de Deseos", "Sfida imperdibile", "バケットリスト", "버킷 리스트", "極限任務", "Seznam výzev", "Bakancslista", "Lista życzeń", "Список гонщика", "Groepslijst"}},
{"Festival", {"Festival", "Festival", "Festival", "Festival", "Festival", "Festival", "フェスティバル", "페스티벌", "嘉年華", "Festival", "Fesztivál", "Festiwal", "Фестиваль", "Festival"}},
{"Barn Find", {"Carro em Celeiro", "Scheunenfund", "Trésor de grange", "Coche abandonado", "Hallazgo en granero", "Gioiello dimenticato", "掘り出し物", "숨겨진 클래식 자동차", "車房寶物", "Nález ve stodole", "Csűrlelet", "Zapomniany wóz", "Раритет", "Schuurvondst"}},
{"Bonus Board", {"Placa de Bônus", "Bonustafel", "Panneau bonus", "Cartel de bonificación", "Cartel de bonus", "Insegna bonus", "ボーナス ボード", "보너스 표지판", "獎勵看板", "Bonusová tabule", "Bónusztábla", "Tablica premiowa", "Призовой стенд", "Bonusbord"}},
{"Start Matchmaking for", {"Buscar partida em", "Matchmaking starten für", "Rechercher une partie", "Buscar partida en", "Buscar partida en", "Avvia matchmaking per", "マッチメイキング開始", "매치메이킹 시작", "開始配對", "Hledat hru", "Párkeresés indítása", "Szukaj gry", "Начать поиск игры", "Matchmaking starten voor"}},
{"Online Road Trip", {"Pegar a Estrada Online", "Online-Autotour", "Road trip en ligne", "Viaje por carretera en línea", "Viaje online", "Grand Tour online", "オンライン ロード トリップ", "온라인 로드 트립", "線上公路旅行", "Online Road Trip", "Online Road Trip", "Wycieczka online", "\"Большие гонки\" в сети", "Online roadtrip"}},
{"Online Freeroam", {"Passeio Livre Online", "Freie Fahrt online", "Balade libre en ligne", "Conducción libre en línea", "Modo libre online", "Viaggio libero online", "オンライン フリー走行", "온라인 자유 주행", "線上漫遊模式", "Online volná jízda", "Online szabad kocsikázás", "Jazda dowolna online", "Свободный заезд в сети", "Online vrij rondrijden"}},
{"Private Online Road Trip", {"Pegar a Estrada Online Particular", "Private Online-Autotour", "Road trip en ligne privé", "Viaje por carretera en línea privado", "Viaje online privado", "Grand Tour online privato", "プライベート オンライン ロード トリップ", "비공개 온라인 로드 트립", "私人線上公路旅行", "Soukromý online Road Trip", "Privát online Road Trip", "Prywatna wycieczka online", "Частные \"Большие гонки\" в сети", "Online roadtrip privé"}},
{"Private Online Freeroam", {"Passeio Livre Online Particular", "Private freie Fahrt online", "Balade libre en ligne privée", "Conducción libre en línea privada", "Modo libre online privado", "Viaggio libero online privato", "プライベート オンライン フリー走行", "비공개 온라인 자유 주행", "私人線上漫遊模式", "Soukromá online volná jízda", "Privát online szabad kocsikázás", "Prywatna jazda dowolna online", "Частный свободный заезд в сети", "Online vrij rondrijden privé"}},
// the Mods page
{"Autodrive cinematic camera", {"Câmera cinematográfica na direção automática", "Kinokamera beim Autopiloten", "Caméra cinématique en conduite auto", "Cámara cinemática en conducción automática", "Cámara cinemática en manejo automático", "Telecamera cinematografica in guida automatica", "自動運転のシネマカメラ", "자동 주행 시네마틱 카메라", "自動駕駛電影鏡頭", "Filmová kamera při autopilotu", "Filmes kamera robotpilótánál", "Kamera filmowa na autopilocie", "Кинокамера на автопилоте", "Filmische camera bij automatisch rijden"}},
{"A few seconds into ANNA's autodrive, the camera turns to slow orbiting shots of the car and the HUD hides. Taking over brings them back.",
 {"Alguns segundos depois de a ANNA assumir a direção, a câmera passa a girar lentamente em volta do carro e o HUD some. Ao retomar o controle, tudo volta.",
  "Ein paar Sekunden nachdem ANNA übernommen hat, kreist die Kamera langsam um das Auto und das HUD verschwindet. Wenn du wieder selbst fährst, kommt beides zurück.",
  "Quelques secondes après le début de la conduite auto d'ANNA, la caméra tourne lentement autour de la voiture et le HUD disparaît. Reprendre le volant les rétablit.",
  "Unos segundos después de que ANNA empiece a conducir, la cámara gira lentamente alrededor del coche y el HUD se oculta. Al retomar el control, todo vuelve.",
  "Unos segundos después de que ANNA empiece a manejar, la cámara gira lentamente alrededor del auto y el HUD se oculta. Al retomar el control, todo vuelve.",
  "Pochi secondi dopo l'inizio della guida automatica di ANNA, la telecamera ruota lentamente intorno all'auto e l'HUD si nasconde. Riprendendo il controllo tornano entrambi.",
  "ANNAの自動運転が始まって数秒後、カメラが車の周りをゆっくり回り、HUDが隠れます。操作を戻すと元に戻ります。",
  "ANNA의 자동 주행이 시작되고 몇 초 후, 카메라가 차량 주위를 천천히 돌고 HUD가 숨겨집니다. 직접 운전하면 다시 돌아옵니다.",
  "ANNA 自動駕駛開始幾秒後，鏡頭會緩緩環繞車輛，HUD 也會隱藏。接手駕駛後即恢復。",
  "Pár sekund po spuštění autopilota ANNA se kamera začne pomalu otáčet kolem auta a HUD se skryje. Převzetím řízení se vše vrátí.",
  "Néhány másodperccel azután, hogy ANNA átveszi a vezetést, a kamera lassan körbejárja az autót, és a HUD eltűnik. Ha újra te vezetsz, minden visszaáll.",
  "Kilka sekund po włączeniu autopilota ANNA kamera zaczyna powoli krążyć wokół samochodu, a HUD znika. Przejęcie kierownicy przywraca widok.",
  "Через несколько секунд после включения автопилота ANNA камера начинает медленно облетать машину, а HUD скрывается. Если снова взять управление, всё вернётся.",
  "Een paar seconden nadat ANNA automatisch gaat rijden, draait de camera langzaam rond de auto en verdwijnt de HUD. Neem je het stuur weer over, dan komen ze terug."}},
{"Hide hood and trunk in ANNA", {"Ocultar capô e porta-malas na ANNA", "Motorhaube und Kofferraum in ANNA ausblenden", "Masquer le capot et le coffre dans ANNA", "Ocultar capó y maletero en ANNA", "Ocultar cofre y cajuela en ANNA", "Nascondi cofano e bagagliaio in ANNA", "ANNAでボンネットとトランクを隠す", "ANNA에서 후드과 트렁크 숨기기", "在 ANNA 中隱藏引擎蓋和後車箱", "Skrýt kapotu a kufr v ANNA", "Motorháztető és csomagtartó elrejtése az ANNA-ban", "Ukryj maskę i bagażnik w ANNA", "Скрыть капот и багажник в ANNA", "Motorkap en kofferbak verbergen in ANNA"}},
{"Car > Doors leaves out the hood and the trunk, and Open All leaves them shut: for cars with an invisible trunk or a fake engine under the hood.",
 {"Carro > Portas deixa de fora o capô e o porta-malas, e Abrir tudo os mantém fechados: para carros com porta-malas invisível ou motor falso sob o capô.",
  "Auto > Türen lässt Motorhaube und Kofferraum weg, und Alle öffnen lässt sie zu: für Autos mit unsichtbarem Kofferraum oder einem falschen Motor unter der Haube.",
  "Voiture > Portières omet le capot et le coffre, et Tout ouvrir les laisse fermés : pour les voitures au coffre invisible ou au faux moteur sous le capot.",
  "Coche > Puertas no incluye el capó ni el maletero, y Abrir todo los deja cerrados: para coches con maletero invisible o un motor falso bajo el capó.",
  "Auto > Puertas no incluye el cofre ni la cajuela, y Abrir todo los deja cerrados: para autos con cajuela invisible o un motor falso bajo el cofre.",
  "Auto > Portiere esclude cofano e bagagliaio, e Apri tutto li lascia chiusi: per le auto con bagagliaio invisibile o un motore finto sotto il cofano.",
  "車 > ドア にボンネットとトランクを表示せず、すべて開ける でも閉じたままにします。トランクが見えない車や、ボンネットの下のエンジンが作り物の車向けです。",
  "자동차 > 도어에서 후드과 트렁크를 빼고, 모두 열기에서도 닫아 둡니다. 트렁크가 보이지 않거나 후드 아래 엔진이 가짜인 차량용입니다.",
  "車輛 > 車門 不列出引擎蓋和後車箱，全部打開 也不會打開它們：適用於後車箱不可見或引擎蓋下沒有真引擎的車輛。",
  "Auto > Dveře vynechá kapotu a kufr a Otevřít vše je nechá zavřené: pro auta s neviditelným kufrem nebo falešným motorem pod kapotou.",
  "Az Autó > Ajtók kihagyja a motorháztetőt és a csomagtartót, az Összes nyitása pedig zárva hagyja őket: láthatatlan csomagtartójú vagy álmotoros autókhoz.",
  "Samochód > Drzwi pomija maskę i bagażnik, a Otwórz wszystko zostawia je zamknięte: dla aut z niewidocznym bagażnikiem lub atrapą silnika pod maską.",
  "Машина > Двери не показывает капот и багажник, а \"Открыть всё\" оставляет их закрытыми: для машин с невидимым багажником или ненастоящим двигателем под капотом.",
  "Auto > Portieren laat motorkap en kofferbak weg, en Alles openen houdt ze dicht: voor auto's met een onzichtbare kofferbak of een nepmotor onder de kap."}},
{"Close doors when moving", {"Fechar portas ao andar", "Türen beim Losfahren schließen", "Fermer les portières en roulant", "Cerrar puertas al moverse", "Cerrar puertas al moverse", "Chiudi le portiere in movimento", "走り出したらドアを閉める", "주행 시 도어 닫기", "行駛時關閉車門", "Zavřít dveře při jízdě", "Ajtók zárása induláskor", "Zamykaj drzwi podczas jazdy", "Закрывать двери при движении", "Portieren sluiten bij wegrijden"}},
{"The doors, hood, trunk and storage ANNA opened shut by themselves once the car drives off. The roof stays as it is.",
 {"As portas, o capô, o porta-malas e os compartimentos abertos pela ANNA se fecham sozinhos quando o carro anda. O teto fica como está.",
  "Türen, Motorhaube, Kofferraum und Staufächer, die ANNA geöffnet hat, schließen sich von selbst, sobald das Auto losfährt. Das Dach bleibt, wie es ist.",
  "Les portières, le capot, le coffre et les rangements ouverts par ANNA se ferment d'eux-mêmes dès que la voiture roule. Le toit reste tel quel.",
  "Las puertas, el capó, el maletero y los compartimentos que abrió ANNA se cierran solos cuando el coche arranca. El techo se queda como está.",
  "Las puertas, el cofre, la cajuela y los compartimentos que abrió ANNA se cierran solos cuando el auto arranca. El techo se queda como está.",
  "Portiere, cofano, bagagliaio e vani aperti da ANNA si chiudono da soli appena l'auto parte. Il tetto resta com'è.",
  "ANNAで開けたドア、ボンネット、トランク、収納は、車が走り出すと自動で閉まります。ルーフはそのままです。",
  "ANNA로 연 도어, 후드, 트렁크, 수납공간은 차량이 출발하면 자동으로 닫습니다. 지붕은 그대로 둡니다.",
  "ANNA 打開的車門、引擎蓋、後車箱和置物箱會在車輛起步時自動關閉。車頂維持原樣。",
  "Dveře, kapota, kufr a úložné prostory otevřené ANNA se samy zavřou, jakmile se auto rozjede. Střecha zůstane, jak je.",
  "Az ANNA által kinyitott ajtók, motorháztető, csomagtartó és tárolórekeszek maguktól bezáródnak, amint az autó elindul. A tető marad, ahogy van.",
  "Drzwi, maska, bagażnik i schowki otwarte przez ANNA zamykają się same, gdy samochód ruszy. Dach zostaje bez zmian.",
  "Двери, капот, багажник и отсеки, открытые ANNA, закрываются сами, как только машина трогается. Крыша остаётся как есть.",
  "Portieren, motorkap, kofferbak en opbergvakken die ANNA opende, gaan vanzelf dicht zodra de auto wegrijdt. Het dak blijft zoals het is."}},
{"Start Matchmaking in ANNA", {"Buscar partida na ANNA", "Matchmaking in ANNA starten", "Rechercher une partie dans ANNA", "Buscar partida en ANNA", "Buscar partida en ANNA", "Avvia matchmaking in ANNA", "ANNAでマッチメイキング開始", "ANNA에서 매치메이킹 시작", "在 ANNA 中開始配對", "Hledat hru v ANNA", "Párkeresés az ANNA-ban", "Szukaj gry w ANNA", "Поиск игры в ANNA", "Matchmaking starten in ANNA"}},
{"ANNA's \"Start Matchmaking for\" list: Online Road Trip, Online Freeroam, and their private sessions, started without the pause menu.",
 {"A lista \"Buscar partida em\" da ANNA: Pegar a Estrada Online, Passeio Livre Online e suas sessões particulares, iniciadas sem o menu de pausa.",
  "ANNAs Liste \"Matchmaking starten für\": Online-Autotour, freie Fahrt online und ihre privaten Sitzungen, ohne das Pausenmenü gestartet.",
  "La liste \"Rechercher une partie\" d'ANNA : road trip en ligne, balade libre en ligne et leurs sessions privées, lancés sans le menu pause.",
  "La lista \"Buscar partida en\" de ANNA: viaje por carretera en línea, conducción libre en línea y sus sesiones privadas, sin el menú de pausa.",
  "La lista \"Buscar partida en\" de ANNA: viaje online, modo libre online y sus sesiones privadas, sin el menú de pausa.",
  "L'elenco \"Avvia matchmaking per\" di ANNA: Grand Tour online, viaggio libero online e le loro sessioni private, avviati senza il menu di pausa.",
  "ANNAの「マッチメイキング開始」リスト: オンライン ロード トリップ、オンライン フリー走行、それぞれのプライベート セッションを、ポーズメニューを開かずに開始します。",
  "ANNA의 \"매치메이킹 시작\" 목록: 온라인 로드 트립, 온라인 자유 주행 및 각각의 비공개 세션을 일시 정지 메뉴 없이 시작합니다.",
  "ANNA 的「開始配對」清單：線上公路旅行、線上漫遊模式及其私人遊戲，不必打開暫停選單即可開始。",
  "Seznam ANNA „Hledat hru“: Online Road Trip, online volná jízda a jejich soukromé relace, spuštěné bez menu pauzy.",
  "Az ANNA „Párkeresés indítása” listája: Online Road Trip, online szabad kocsikázás és a privát változataik, a szünetmenü nélkül.",
  "Lista „Szukaj gry” w ANNA: wycieczka online, jazda dowolna online i ich prywatne sesje, uruchamiane bez menu pauzy.",
  "Список ANNA \"Начать поиск игры\": \"Большие гонки\" в сети, свободный заезд в сети и их частные сессии, без меню паузы.",
  "ANNA's lijst \"Matchmaking starten voor\": online roadtrip, online vrij rondrijden en hun privésessies, gestart zonder het pauzemenu."}},
};
// clang-format on

// Czech and Hungarian: the game shows its own text in English there (their string tables are
// English), and its fonts there (Anthem's alone) may not have their letters: ANNA's English too.
// (The translations are kept for when that changes.)
bool english_there(int i) { return !std::strcmp(kCodes[i], "CZ") || !std::strcmp(kCodes[i], "HU"); }

std::wstring g_ini;
std::atomic<int> g_saved{-2};   // the language last seen (an index, -1 English), from the ini; -2 unread

// The game's text language: an index into kCodes, -1 English, -2 not known yet.
int game_language() {
    __try {
        auto* manager = *reinterpret_cast<uint8_t**>(g_base + RVA_STRINGS);
        if (!manager || !readable(manager + 0x30, 32)) return -2;
        const uint8_t* name = manager + 0x30;
        std::size_t count, capacity;
        std::memcpy(&count, name + 16, sizeof count);
        std::memcpy(&capacity, name + 24, sizeof capacity);
        if (capacity != 15 || count < 2 || count > 3 || name[count]) return -2;
        for (int i = 0; i < kLangs; ++i)
            if (!_stricmp(reinterpret_cast<const char*>(name), kCodes[i])) return english_there(i) ? -1 : i;
        return -1;   // EN, GB, DEV, LOC
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -2;
    }
}

int language() {
    const int now = g_base ? game_language() : -2;
    if (now == -2) return g_saved.load() < -1 ? -1 : g_saved.load();
    if (now != g_saved.exchange(now) && !g_ini.empty())   // for the Mods page next time
        WritePrivateProfileStringW(L"Language", L"Last", now < 0 ? L"EN" : wide(kCodes[now]).c_str(), g_ini.c_str());
    return now;
}

} // namespace

void lang_init(const std::wstring& ini) {
    g_ini           = ini;
    wchar_t code[8] = {};
    GetPrivateProfileStringW(L"Language", L"Last", L"EN", code, 8, ini.c_str());
    int saved = -1;
    for (int i = 0; i < kLangs; ++i)
        if (!_wcsicmp(code, wide(kCodes[i]).c_str()) && !english_there(i)) saved = i;
    g_saved = saved;
}

const char* tr(const char* english) {
    if (!english) return english;
    const int l = language();
    if (l < 0) return english;
    for (const Text& t : kTexts)
        if (!std::strcmp(t.en, english)) return t.t[l];
    return english;
}

std::wstring wide(const char* utf8) {
    if (!utf8 || !*utf8) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w.data(), n);
    return w;
}

} // namespace fh2anna
