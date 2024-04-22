#include <M5Dial.h>

#define WIFI_SSID     "xxxxx"
#define WIFI_PASSWORD "xxxxxxxxxx"
#define NTP_TIMEZONE  "UTC-8"
#define NTP_SERVER1   "0.pool.ntp.org"
#define NTP_SERVER2   "1.pool.ntp.org"
#define NTP_SERVER3   "2.pool.ntp.org"

#include <WiFi.h>

#if __has_include(<esp_sntp.h>)
#include <esp_sntp.h>
#define SNTP_ENABLED 1
#elif __has_include(<sntp.h>)
#include <sntp.h>
#define SNTP_ENABLED 1
#endif


char buf[100];

int alarmon = 0;  // アラームオンオフ（1＝オン）
int alhour = 0; // アラーム設定（時間）
int almin = 0;  // アラーム設定（分）
int alarming = 0;
int alcolor = CYAN;
int curcolor = DARKCYAN;
int bkcolor = BLACK;
int nowhour = 0;
int nowmin = 0;
time_t ttalarm;
time_t ttold;
int sx, sy;
int blink = 0;  // 時刻ブリンク用

int addtime = 60;
int redraw = 0;

long oldPosition = -999;

// 初期設定
void setup() {
	// put your setup code here, to run once:
	
	auto cfg = M5.config();
	M5Dial.begin(cfg, true, true);
	
	M5Dial.Display.fillScreen(bkcolor);
	M5Dial.Display.setTextColor(GREEN, bkcolor);
	M5Dial.Display.setTextDatum(middle_center);
	M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
	M5Dial.Display.setTextSize(0.5);
	
	sx = M5Dial.Display.width() / 2;
	sy = M5Dial.Display.height() / 2;
	
	// WiFi接続
	M5Dial.Display.fillScreen(bkcolor);
	M5Dial.Display.drawString("WiFi connecting...", sx, sy);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	int tout = 0;
	while(WiFi.status() != WL_CONNECTED){
		delay(500);
		tout++;
		if(tout > 10){  // 5秒でタイムアウト、接続リトライ
			WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
			tout = 0;
		}
	}
	Serial.println("WiFi Connected.");
	M5Dial.Display.fillScreen(bkcolor);
	M5Dial.Display.drawString("Connected.", sx, sy);
	
	// NTP
	configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
	tout = 0;
	while(sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED){
		delay(1000);
		tout++;
		if(tout > 10){  // 10秒でタイムアウト、接続リトライ
			configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
			tout = 0;
		}
	}
	M5Dial.Display.fillScreen(bkcolor);
	M5Dial.Display.drawString("NTP Connected.", sx, sy);
	
	// RTC
	time_t tt = time(nullptr) + 1;  // Advance one second.
	while(tt > time(nullptr)){
		;  /// Synchronization in seconds
	}
	tt += (9 * 60 * 60);  // JPN
	M5Dial.Rtc.setDateTime(gmtime(&tt));
	ttold = tt;
	Serial.printf("TT = %ld\n", tt);
	
	// 秒の桁を00にする（アラーム時間としてセット）
	struct tm *tm = gmtime(&tt);
	tm->tm_sec = 0;  // 秒を00にする
	alarmon = 0;
	alhour = tm->tm_hour;
	almin = tm->tm_min;
	tt = mktime(tm) + (8 * 60 * 60);
	ttalarm = tt;
	Serial.printf("ALARM = %ld\n", ttalarm);
	
	auto dt = M5Dial.Rtc.getDateTime();
	sprintf(buf, 
		"%04d/%02d/%02d %02d:%02d:%02d\r\n",
		dt.date.year, dt.date.month, dt.date.date,
		dt.time.hours, dt.time.minutes,
		dt.time.seconds
	);
	
	M5Dial.Display.fillScreen(bkcolor);
	M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
	M5Dial.Display.setTextColor(WHITE, bkcolor);
	M5Dial.Display.setTextSize(0.5);
	M5Dial.Display.drawString("ALARM", sx, sy-80);
	M5Dial.Display.fillRect(sx+8, sy+35+32, 60, 4, curcolor);
}

// m5::rtc_datetime_t -> time_t 変換
time_t dt2tt(m5::rtc_datetime_t dt)
{
	struct tm tm;
	tm.tm_year  = dt.date.year - 1900;    // 年 [1900からの経過年数]
	tm.tm_mon   = dt.date.month - 1;   // 月 [0-11] 0から始まることに注意
	tm.tm_mday  = dt.date.date;    // 日 [1-31]
	tm.tm_wday  = dt.date.weekDay; // 曜日 [0:日 1:月 ... 6:土]
	tm.tm_hour  = dt.time.hours;   // 時 [0-23]
	tm.tm_min   = dt.time.minutes; // 分 [0-59]
	tm.tm_sec   = dt.time.seconds; // 秒 [0-61]
	tm.tm_isdst = -1;              // 夏時間フラグ
	
	time_t tt = mktime(&tm) + (8 * 60 * 60);
	return(tt);
}

// メインループ
void loop() {
	// put your main code here, to run repeatedly:
	M5Dial.update();
	
	// ボタン長押し（セット＆解除）-> 放したタイミングで確定
    if(M5Dial.BtnA.pressedFor(1000)){
		if(redraw == 0){
			if(alarmon == 0){
				if(alarming != 0){
					bkcolor = PURPLE;
					M5Dial.Display.fillScreen(bkcolor);
					M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
					M5Dial.Display.setTextColor(WHITE, bkcolor);
					M5Dial.Display.setTextSize(0.5);
					M5Dial.Display.drawString("ALARM", sx, sy-80);
					// 時刻のみ先に表示
					M5Dial.Display.setTextFont(7);
					M5Dial.Display.setTextColor(GREEN, bkcolor);
					M5Dial.Display.setTextSize(1);
					sprintf(buf, "%02d:%02d", nowhour, nowmin);
					M5Dial.Display.drawString(buf, sx, sy-35);
				}
				alarmon = 1;
			}
			else{
				bkcolor = BLACK;
				M5Dial.Display.fillScreen(bkcolor);
				M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
				M5Dial.Display.setTextColor(WHITE, bkcolor);
				M5Dial.Display.setTextSize(0.5);
				M5Dial.Display.drawString("ALARM", sx, sy-80);
				// 時刻のみ先に表示
				M5Dial.Display.setTextFont(7);
				M5Dial.Display.setTextColor(GREEN, bkcolor);
				M5Dial.Display.setTextSize(1);
				sprintf(buf, "%02d:%02d", nowhour, nowmin);
				M5Dial.Display.drawString(buf, sx, sy-35);
				
				alarmon = 0;
			}
			redraw = 1;
		}
	}
	if(M5Dial.BtnA.wasReleased()){
		if(redraw == 0){  // 長押しで放した時はキャンセル
			if(addtime == 60){
				addtime = 3600;
				M5Dial.Display.fillRect(sx+8, sy+35+32, 60, 4, bkcolor);
				M5Dial.Display.fillRect(sx-68, sy+35+32, 60, 4, curcolor);
			}
			else{
				addtime = 60;
				M5Dial.Display.fillRect(sx+8, sy+35+32, 60, 4, curcolor);
				M5Dial.Display.fillRect(sx-68, sy+35+32, 60, 4, bkcolor);
			}
		}
		redraw = 0;
	}
	
	// タッチでもアラーム中解除
	auto t = M5Dial.Touch.getDetail();
	if(t.state == 3){  // touch_begin
		if(alarmon != 0 && alarming != 0){
			bkcolor = BLACK;
			M5Dial.Display.fillScreen(bkcolor);
			M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
			M5Dial.Display.setTextColor(WHITE, bkcolor);
			M5Dial.Display.setTextSize(0.5);
			M5Dial.Display.drawString("ALARM", sx, sy-80);
			// 時刻のみ先に表示
			M5Dial.Display.setTextFont(7);
			M5Dial.Display.setTextColor(GREEN, bkcolor);
			M5Dial.Display.setTextSize(1);
			sprintf(buf, "%02d:%02d", nowhour, nowmin);
			M5Dial.Display.drawString(buf, sx, sy-35);
			
			alarmon = 0;
			redraw = 1;
		}
	}
	
	
	// ダイアル処理
	time_t ttnew = ttalarm;
	long newPosition = M5Dial.Encoder.read();
	if(newPosition > oldPosition){
		ttnew += addtime;  // +1分
		oldPosition = newPosition;
	}
	else if(newPosition < oldPosition){
		ttnew -= addtime;  // -1分
		oldPosition = newPosition;
	}
	
	if(ttnew != ttalarm || redraw != 0){
		// アラーム時間
		ttalarm = ttnew;
		struct tm *tm = gmtime(&ttalarm);
		alhour = tm->tm_hour;
		almin = tm->tm_min;
		
		M5Dial.Display.setTextSize(1);
		if(alarmon == 0){
			alcolor = CYAN;
			curcolor = DARKCYAN;
		}
		else{
			alcolor = RED;
			curcolor = MAROON;
		}
		
		M5Dial.Display.setTextColor(alcolor, bkcolor);
		M5Dial.Display.setTextFont(7);
		struct tm *tmalarm = gmtime(&ttalarm);
		sprintf(buf, "%02d:%02d", tmalarm->tm_hour, tmalarm->tm_min);
		M5Dial.Display.drawString(buf, sx, sy+35);
		
		if(addtime == 60){
			M5Dial.Display.fillRect(sx+8, sy+35+32, 60, 4, curcolor);
		}
		else{
			M5Dial.Display.fillRect(sx-68, sy+35+32, 60, 4, curcolor);
		}
		
		if(redraw == 0){
			// ダイアル開店時のみ音を鳴らす
			M5Dial.Speaker.tone(1000, 20);
		}
	}
	
	auto dt = M5Dial.Rtc.getDateTime();
	time_t tt = dt2tt(dt);
	
	// 20億超えと+-60秒以上とマイナスは無視する
	if(tt != ttold){
		if(tt > 2000000000){
			tt = ttold;
		}
		else if(tt < 0){
			tt = ttold;
		}
		else if(((tt - ttold) > 60) || ((tt - ttold) < -60)){
			tt = ttold;
		}
	}
	
	if(tt > ttold){
		// 時間更新
		M5Dial.Display.setTextSize(1);
		// 現在時間
		M5Dial.Display.setTextColor(GREEN, bkcolor);
		M5Dial.Display.setTextFont(7);
		if(blink == 0){
			sprintf(buf, "%02d:%02d", dt.time.hours, dt.time.minutes);
			blink = 1;
		}
		else{
			sprintf(buf, "%02d %02d", dt.time.hours, dt.time.minutes);
			blink = 0;
		}
		M5Dial.Display.drawString(buf, sx, sy-35);
		
		// アラーム時間
		if(alhour == dt.time.hours && almin == dt.time.minutes){
			if(alarmon != 0){
				if(alarming == 0){
					// アラーム開始
					alarmdisp(1, dt.time.hours, dt.time.minutes);
				}
				alarming = 1;
				M5Dial.Speaker.tone(4000, 100);
				delay(100);
				M5Dial.Speaker.tone(4000, 100);
			}
		}
		else{
			if(alarming != 0){
				// アラーム終了
				alarmdisp(0, dt.time.hours, dt.time.minutes);
			}
			alarming = 0;
		}
		
		nowhour = dt.time.hours;
		nowmin = dt.time.minutes;
		ttold = tt;
	}
	else{
		delay(20);
	}
	
}

// アラーム時の表示変更
void alarmdisp(int onoff, int hour, int min)
{
	char buf[100];
	
	if(onoff != 0){
		bkcolor = PURPLE;
	}
	else{
		bkcolor = BLACK;
	}
	M5Dial.Display.fillScreen(bkcolor);
	M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
	M5Dial.Display.setTextColor(WHITE, bkcolor);
	M5Dial.Display.setTextSize(0.5);
	M5Dial.Display.drawString("ALARM", sx, sy-80);
	// 時刻表示
	M5Dial.Display.setTextFont(7);
	M5Dial.Display.setTextColor(GREEN, bkcolor);
	M5Dial.Display.setTextSize(1);
	sprintf(buf, "%02d:%02d", hour, min);
	M5Dial.Display.drawString(buf, sx, sy-35);
	// アラーム時刻表示
	M5Dial.Display.setTextColor(alcolor, bkcolor);
	struct tm *tmalarm = gmtime(&ttalarm);
	sprintf(buf, "%02d:%02d", tmalarm->tm_hour, tmalarm->tm_min);
	M5Dial.Display.drawString(buf, sx, sy+35);
	if(addtime == 60){
		M5Dial.Display.fillRect(sx+8, sy+35+32, 60, 4, curcolor);
	}
	else{
		M5Dial.Display.fillRect(sx-68, sy+35+32, 60, 4, curcolor);
	}
}
