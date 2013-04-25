#!/bin/bash

TTS=espeak
TTS_OPTS="-a90 -b1 -s130 -p77"

######################
# directories for voice files
######################

rm -rf pl/epeak
mkdir -p pl/espeak/digits/HD
mkdir -p pl/espeak/time/HD
mkdir -p pl/espeak/digits/16000
mkdir -p pl/espeak/time/16000
mkdir -p pl/espeak/digits/8000
mkdir -p pl/espeak/time/8000

######################
# create sound files
######################

function mksound {
    local TYPE=$1; shift
    local FILE=$1; shift
    local TEXT=$*

    ${TTS} ${TTS_OPTS} -v pl -w pl/espeak/${TYPE}/HD/${FILE}.wav "${TEXT}"
    sox pl/espeak/${TYPE}/HD/${FILE}.wav -r 16000 pl/espeak/${TYPE}/16000/${FILE}.wav
    sox pl/espeak/${TYPE}/HD/${FILE}.wav -r 8000 pl/espeak/${TYPE}/8000/${FILE}.wav

    echo "${TYPE} ${FILE}... done."
}

######################
# IVR messages
######################

TYPE=digits

while read FILE TEXT
do
    if [ "${FILE}" != "#" ]
    then
        mksound ${TYPE} ${FILE} ${TEXT}
    else
	echo "Comment: ${TEXT}"
    fi
done <<'EOT'
# l. główne, r.męski, mianownik
0	zero
1	jeden
2	dwa
3	trzy
4	cztery
5	pięć
6	sześć
7	siedem
8	osiem
9	dziewięć
10	dziesięć
11	jedenaście
12	dwanaście
13	trzynaście
14	czternaście
15	pietnaście
16	szesnaście
17	siedemnaście
18	osiemnaście
19	dziewiętnaście
20	dwadzieścia
30	trzydzieści
40	czterdzieści
50	pięćdziesiąt
60	sześćdziesiąt
70	siedemdziesiąt
80	osiemdziesiąc
90	dziewięćdziesiąt
100	sto
200	dwieście
300	trzysta
400	czterysta
500	pięćset
600	sześćset
700	siedemset
800	osiemset
900	dziewięćset
1000	tysiąc
1000a	tysiące
1000s	tysięcy
1000000	milion
1000000a	miliony
1000000s	milionów
# l. główne, r.żeński, mianownik
1_f	jedna
2_f	dwie
# l. główne, r.nijaki, mianownik
1_n	jedno
2_n	dwie
# l. porządowe, r.męski, mianownik
0_pm	zerowy
1_pm	pierwszy
2_pm	drugi
3_pm	trzeci
4_pm	czwarty
5_pm	piąty
6_pm	szósty
7_pm	siódmy
8_pm	ósmy
9_pm	dziewiąty
10_pm	dziesiąty
11_pm	jedenasty
12_pm	dwunasty
13_pm	trzynasty
14_pm	czternasty
15_pm	piętnasty
16_pm	szesnasty
17_pm	siedemnasty
18_pm	osiemnasty
19_pm	dziewiętnasty
20_pm	dwudziesty
30_pm	trzydziesty
40_pm	czterdziesty
50_pm	pięćdziesiąty
60_pm	sześćdziesiąty
70_pm	siedemdziesiąty
80_pm	osiemdziesiąty
90_pm	dziewięćdziesiąty
100_pm	setny
200_pm	dwusetny
300_pm	trzysetny
400_pm	czterysetny
500_pm	pięćsetny
600_pm	sześćsetny
700_pm	siedemsetny
800_pm	osiemsetny
900_pm	dziewięćsetny
1000_pm	tysięczny
# l. porządowe specjane - numery dni, r.męski, mianownik
01_pm	pierwszy
02_pm	drugi
03_pm	trzeci
04_pm	czwarty
05_pm	piąty
06_pm	szósty
07_pm	siódmy
08_pm	ósmy
09_pm	dziewiąty
21_pm	dwudziesty pierwszy
22_pm	dwudziesty drugi
23_pm	dwudziesty trzeci
24_pm	dwudziesty czwarty
25_pm	dwudziesty piąty
26_pm	dwudziesty szósty
27_pm	dwudziesty siódmy
28_pm	dwudziesty ósmy
29_pm	dwudziesty dziewiąty
31_pm	trzydziesty pierwszy

# l. porządowe, r.żeski, mianownik
0_pf	zerowa
1_pf	pierwsza
2_pf	druga
3_pf	trzecia
4_pf	czwarta
5_pf	piąta
6_pf	szósta
7_pf	siódma
8_pf	ósma
9_pf	dziewiąta
10_pf	dziesiąta
11_pf	jedenasta
12_pf	dwunasta
13_pf	trzynasta
14_pf	czternasta
15_pf	piętnasta
16_pf	szesnasta
17_pf	siedemnasta
18_pf	osiemnasta
19_pf	dziewiętnasta
20_pf	dwudziesta
30_pf	trzydziesta
40_pf	czterdziesta
50_pf	pięćdziesiąta
60_pf	sześćdziesiąta
70_pf	siedemdziesiąta
80_pf	osiemdziesiąta
90_pf	dziewięćdziesiąta
100_pf	setna
200_pf	dwusetna
300_pf	trzysetna
400_pf	czterysetna
500_pf	pięćsetna
600_pf	sześćsetna
700_pf	siedemsetna
800_pf	osiemsetna
900_pf	dziewięćsetna
1000_pf	tysięczna
# l. porządowe, r.męski, dopełniacz
0_pmD	zerowego
1_pmD	pierwszego
2_pmD	drugiego
3_pmD	trzeciego
4_pmD	czwartego
5_pmD	piątego
6_pmD	szóstego
7_pmD	siódmego
8_pmD	ósmego
9_pmD	dziewiątego
10_pmD	dziesiątego
11_pmD	jedenastego
12_pmD	dwunastego
13_pmD	trzynastego
14_pmD	czternastego
15_pmD	piętnastego
16_pmD	szesnastego
17_pmD	siedemnastego
18_pmD	osiemnastego
19_pmD	dziewiętnastego
20_pmD	dwudziestego
30_pmD	trzydziestego
40_pmD	czterdziestego
50_pmD	pięćdziesiątego
60_pmD	sześćdziesiątego
70_pmD	siedemdziesiątego
80_pmD	osiemdziesiątego
90_pmD	dziewięćdziesiątego
100_pmD	setnego
200_pmD	dwusetnego
300_pmD	trzysetnego
400_pmD	czterysetnego
500_pmD	pięćsetnego
600_pmD	sześćsetnego
700_pmD	siedemsetnego
800_pmD	osiemsetnego
900_pmD	dziewięćsetnego
1000_pmD	tysięcznego
# l. porządowe, r.żeski, dopełniacz
0_pfD	zerowej
1_pfD	pierwszej
2_pfD	drugiej
3_pfD	trzeciej
4_pfD	czwartej
5_pfD	piątej
6_pfD	szóstej
7_pfD	siódmej
8_pfD	ósmej
9_pfD	dziewiątej
10_pfD	dziesiątej
11_pfD	jedenastej
12_pfD	dwunastej
13_pfD	trzynastej
14_pfD	czternastej
15_pfD	piętnastej
16_pfD	szesnastej
17_pfD	siedemnastej
18_pfD	osiemnastej
19_pfD	dziewiętnastej
20_pfD	dwudziestej
30_pfD	trzydziestej
40_pfD	czterdziestej
50_pfD	pięćdziesiątej
60_pfD	sześćdziesiątej
70_pfD	siedemdziesiątej
80_pfD	osiemdziesiątej
90_pfD	dziewięćdziesiątej
100_pfD	setnej
200_pfD	dwusetnej
300_pfD	trzysetnej
400_pfD	czterysetnej
500_pfD	pięćsetnej
600_pfD	sześćsetnej
700_pfD	siedemsetnej
800_pfD	osiemsetnej
900_pfD	dziewięćsetnej
1000_pfD	tysięcznej
# dodatkowe
star	gwiazdka
star_C	gwiazdkę
pound	krzyżyk
dot	kropka
dot_C	kropkę
coma	przecinek
EOT

TYPE=time

while read FILE TEXT
do
    if [ "${FILE}" != "#" ]
    then
        mksound ${TYPE} ${FILE} ${TEXT}
    else
	echo "Comment: ${TEXT}"
    fi
done <<'EOT'
# l. jednostki czasu
t_sekunda	sekunda
t_sekundy	sekundy
t_sekund	sekund
t_minuta	minuta
t_minuty	minuty
t_minut	minut
t_godzina	godzina
t_godziny	godziny
t_godzin	godzin
t_dzien	dzień
t_dni	dni
t_tydzien	tydzień
t_tygodnie	tygodnie
t_tygodni	tygodni
t_miesiac	miesiąc
t_miesiace	miesiące
t_miesiecy	miesięcy
t_rok	rok
t_roku	roku
t_lata	lata
t_lat	lat
# dni tygodnia, mianownik
day-0	niedziela
day-1	poniedziałek
day-2	wtorek
day-3	środa
day-4	czwartek
day-5	piątek
day-6	sobota
# miesiące, mianownik
mon-0	styczeń
mon-1	luty
mon-2	marzec
mon-3	kwiecień
mon-4	maj
mon-5	czerwiec
mon-6	lipiec
mon-7	sierpień
mon-8	wrzesień
mon-9	październik
mon-10	listopad
mon-11	grudzień
# miesiące, dopełniacz
mon-0_D	stycznia
mon-1_D	lutego
mon-2_D	marca
mon-3_D	kwietnia
mon-4_D	maja
mon-5_D	czerwieca
mon-6_D	lipieca
mon-7_D	sierpnia
mon-8_D	września
mon-9_D	października
mon-10_D	listopada
mon-11_D	grudnia
# dodatkowe
t_wczoraj	wczoraj
t_dzisiaj	dzisiaj
t_jutro		jutro
EOT

echo "ALL DONE."