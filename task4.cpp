#include <Wire.h> // I2C通信用の標準ライブラリ
#include <Adafruit_GFX.h> // 図形描画用のライブラリ
#include <Adafruit_LEDBackpack.h> // HT16K33制御用のライブラリ

#define X 8
#define Y 10 //2マス余計にとってバッファとする

#define maxBlockX 2
#define maXBlockY 2

//github実験 これはbranch modify0701

/*todo :  既にあるブロックの角を通る時その角が消えるバグの修正
          図形の追加
          図形の回転
          嘘ブロック（疑心暗鬼要素）次の嘘ブロックは2ライン消すまで出ないとかにしたらバランスいいかも
          */

// 8x8マトリックス用のオブジェクトを作成
Adafruit_8x8matrix matrix = Adafruit_8x8matrix();

int block[X][Y] = {0};
int eraseLine[Y] = {0}; //消すラインのYを1にする
int currentX = 3;
int currentY = 0;
int shape0[maxBlockX][maXBlockY] = {{1,1},{1,1}}; //正方形
int nextFlag = 0; //次のミノを出現させて良いかのフラグ
int counter = 0; //x座標の移動判定は0.2秒ごとに行われy座標は1秒ごとに行うので5回数えてタイミングが重なるときにそろえる
unsigned long previousMillis;

void setup() {
  // HT16K33のI2Cアドレスを指定して通信開始（デフォルトは 0x70 です）
  matrix.begin(0x70);
  
  // 明るさを設定 (0〜15の範囲。最初は控えめに)
  matrix.setBrightness(5);

  previousMillis = millis();
}

void loop() {
  int moveX = 0, stickX = analogRead(A0); //スティックの入力を保存
  int moveY = 0, stickY = analogRead(A1); //moveX,YはstickX,Yの値を見て実際にどの方向に動かすかを保存
  unsigned long currentMillis = millis();

  moveX = wayOfMove(stickX);
  
  drawCurrentBlock(0);
  
  if(currentMillis - previousMillis >= 200){ //200msに一回x方向に1マス動かせる
    counter += 1;
    if(counter > 4){  //1秒に一回y方向に1マス落ちる
      counter = 0;
      moveCurrentBlock(moveX, 1); //y方向にも動かす
    }else{
      moveCurrentBlock(moveX, 0); //x方向にのみ動かす
    }
    previousMillis = currentMillis;
  }
  
  drawCurrentBlock(1);

  draw();

  if (nextFlag == 1) {
    drawCurrentBlock(1); // 一旦消したブロックを、最終位置に書き戻し固定する
    judgeLine();         // そろったラインを消す
    newBlock();          // 新しいブロックの座標をセットする
    nextFlag = 0;        // フラグをリセットして次の落下に備える
  }
}

//描画関数3点セット
void draw(){
  matrix.clear(); // 内部の描画メモリをクリア
  drawPixel();
  matrix.writeDisplay();  
}

/*block配列中のブロックがある場所を光らせるように書き込む*/
void drawPixel(){
  int x = 0;
  int y = 0;

  for(x = 0; x < X; x++){
    for(y = 2; y < Y; y++){ //バッファ分ずれている
      if(block[x][y] == 1){
        matrix.drawPixel(x, y - 2, LED_ON);
      }
    }
  }
}

/*現在動いているブロックを描画したり削除する 引数で書き込みと削除の動作を指定できる*/
void drawCurrentBlock(int mode){  //mode=1:draw mode=0:erase
  int x = 0;
  int y = 0;
  
  for(x = 0; x < maxBlockX; x++){
    for(y = 0; y < maXBlockY; y++){ 
      if(shape0[x][y] == 1){
        if((currentX + x < X) && (currentY + y < Y)){
          block[currentX + x][currentY + y] = mode;
        }
      }
    }
  }
}

/*現在動いているブロックを動かす 底辺かブロックにy座標が接地すると止まる*/
void moveCurrentBlock(int moveX, int timeFlagY){ //moveX:x方向の動き timeFlagY:y方向に動かすかどうか
  if(timeFlagY == 0){ //x方向にのみ動かすとき
    moveCurrentBlockX(moveX);
  }else{
    moveCurrentBlockY(moveCurrentBlockX(moveX), moveX);
  }
}

int moveCurrentBlockX(int moveX){
  int x = 0;
  int y = 0;
  int moveAbleFlagX = 1; // =1:moveOK  =0:moveNOK

  for(x = 0; x < maxBlockX; x++){
    for(y = 0; y < maXBlockY; y++){
      if(shape0[x][y] == 1){
        if(currentX + x + moveX < 0 || currentX + x + moveX > X - 1 || block[currentX + x + moveX][currentY + y] == 1){ //x方向にはみ出すかブロックに接地したらフラグを降ろす
          moveAbleFlagX = 0;
        }
      }
    }
  }

  if(moveX != 0 && moveAbleFlagX == 1){
    currentX += moveX;
    return 1;
  }else{
    return 0;
  }
}

void moveCurrentBlockY(int moveAbleFlagX, int moveX){
  int x = 0;
  int y = 0;
  int moveAbleFlagY = 1; // =1:moveOK  =0:moveNOK

  for(x = 0; x < maxBlockX; x++){
    for(y = 0; y < maXBlockY; y++){
      if(shape0[x][y] == 1){
        if(currentY + y + 1 > Y - 1){ //y方向にはみ出したらフラグを降ろす y座標の+1は変位
          moveAbleFlagY = 0;
        }else if(block[currentX + x][currentY + y + 1] == 1){ //y方向にブロックがある場合
          moveAbleFlagY = 0;
        }
      }
    }
  }

  if(moveAbleFlagY == 1){
    currentY += 1;
  }else{  //動かせなくなったら次のブロック出していいよ
    nextFlag = 1;
  }
}

/*中央上に新しいブロックを生成する*/
void newBlock(){
  currentX = 3;
  currentY = 0;
}

/*左右どちらに動かすか決定する*/
int wayOfMove(int stickX){
  if(stickX < 300){
    return -1;
  }else if(stickX > 724){
    return 1;
  }else{
    return 0;
  }
}

void judgeLine(){
  int x = 0;
  int y = 2;
  int tetris = 1; //このラインを消していいかのフラグ =1:eraseOK =0:eraseNOK

  for(y = 2; y < Y; y++){
    for(x = 0; x < X; x++){
      if(block[x][y] == 0){
        tetris = 0;
        break;
      }
    }
    if(tetris == 1){
      eraseLine[y] = 1;
    }else{
      tetris = 1;
    }
  }

  eraseEffect();

  updateBlock();

  for(int i = 0; i < Y; i++){
    eraseLine[i] = 0;
  }

  draw();
}

//消えるラインを消して1段ずらす
void updateBlock(){
  int y = 2;

  for(y = 2; y < Y; y++){
    if(eraseLine[y] == 1){
      for(int x = 0; x < X; x++){
        for(int yUp = y - 1; yUp >= 0; yUp--){ //消すラインより上を消したところにコピー,繰り返して消すラインより上を一段下げる
          block[x][yUp + 1] = block[x][yUp];
        }
      }
    }
  }

  for(int i = 0; i < X; i++){ //一番上の行を掃除して終わる
    block[i][0] = 0;
  }
}

//消えるラインをチカチカ点滅させる
void eraseEffect(){
  for(int i = 0; i < 7; i++){
    for(int y = 2; y < Y; y++){
      if(eraseLine[y] == 1){
        for(int x = 0; x < X; x++){
          block[x][y] = (i % 2);
        }
      }
    }

    draw();
  
    delay(100);
  }
}