#include <Wire.h> // I2C通信用の標準ライブラリ
#include <Adafruit_GFX.h> // 図形描画用のライブラリ
#include <Adafruit_LEDBackpack.h> // HT16K33制御用のライブラリ

#define X 8
#define Y 10 //2マス余計にとってバッファとする

// #define maxBlockX 2
// #define maXBlockY 2

#define maxBlockSize 4

//github実験 これはbranch modify0701

/*todo :  既にあるブロックの角を通る時その角が消えるバグの修正 maybe ok
          図形の追加  ok
          ゲームオーバー ok
          図形の回転  maybeok
          すぐ下におろす △
          表示を増やす2個使いたい
          嘘ブロック（疑心暗鬼要素）次の嘘ブロックは2ライン消すまで出ないとかにしたらバランスいいかも
          */
          
/*bag     回転させると速く落ちる
          回転させながら落とすと空中で止まる
            多分回転前に接地して回転して固定されている
          */
          
// 8x8マトリックス用のオブジェクトを作成
Adafruit_8x8matrix matrix = Adafruit_8x8matrix();

int block[X][Y] = {0};
int blockBag[7] = {0, 1, 2, 3, 4, 5, 6}; // 7種類のブロックが入った袋
int bagIndex = 7; // 袋の何番目を取り出すか 最初は空っぽ扱いにするため7
int eraseLine[Y] = {0}; //消すラインのYを1にする
int currentX = 3;
int currentY = 0;
int nextFlag = 0; //次のミノを出現させて良いかのフラグ
int counter = 0; //x座標の移動判定は0.2秒ごとに行われy座標は1秒ごとに行うので数えてタイミングが重なるときにそろえる
unsigned long previousMillis;


/*図形の定義*/
int currentShape[maxBlockSize][maxBlockSize] = {0}; //現在操作しているブロック
int currentShapeType;

struct BlockShape{
  int size; //図形の実際の大きさ 例：Oミノなら2*2で定義できるので2,Tミノには最低2*3必要なので3*3必要として3を入れとく
  int shape[maxBlockSize][maxBlockSize];
};

BlockShape shapes[7] = {
  //Oミノ
  {
    2,
    {
      {1,1,0,0},
      {1,1,0,0},
      {0,0,0,0},
      {0,0,0,0}
    }
  },
  //Iミノ
  {
    4,
    {
      {0,0,0,0},
      {1,1,1,1},
      {0,0,0,0},
      {0,0,0,0}
    }
  },
  //Tミノ
  {
    3,
    {
      {0,1,0,0},
      {1,1,1,0},
      {0,0,0,0},
      {0,0,0,0}
    }
  },
  //Sミノ
  {
    3,
    {
      {0,1,1,0},
      {1,1,0,0},
      {0,0,0,0},
      {0,0,0,0}
    }
  },
  //Zミノ
  {
    3,
    {
      {1,1,0,0},
      {0,1,1,0},
      {0,0,0,0},
      {0,0,0,0}
    }
  },
  //Lミノ
  {
    3,
    {
      {1,0,0,0},
      {1,0,0,0},
      {1,1,0,0},
      {0,0,0,0}
    }
  },
  //Jミノ
  {
    3,
    {
      {0,1,0,0},
      {0,1,0,0},
      {1,1,0,0},
      {0,0,0,0},
    }
  }
};

void setup() {
  // HT16K33のI2Cアドレスを指定して通信開始（デフォルトは 0x70 です）
  matrix.begin(0x70);
  
  // 明るさを設定 (0〜15の範囲。最初は控えめに)
  matrix.setBrightness(5);

  previousMillis = millis();

  randomSeed(analogRead(A3)); //使ってないアナログピンのノイズを利用して毎回ランダム値を変化させるらしい

  newBlock(); //最初のブロック生成
}

void loop() {
  int moveX = 0, stickX = analogRead(A0); //スティックの入力を保存 moveX,YはstickX,Yの値を見て実際にどの方向に動かすかを保存
  int moveY = 0, stickY = 1023 - analogRead(A1); //Yは反転させる
  unsigned long currentMillis = millis();

  moveX = wayOfMove(stickX);
  moveY = wayOfMove(stickY);
  
  drawCurrentBlock(0);
  
  if(currentMillis - previousMillis >= 100){
    counter += 1;
    if(counter > 10){  //1秒に一回y方向に1マス落ちる
      counter = 0;
      moveCurrentBlock(moveX, 1); //y方向に自動で動かす(強制)
    }else if(counter % 2 == 0){ //200msに一回x方向に1マス動かせる
      moveCurrentBlock(moveX, moveY); //x方向とy軸方向に動かす
    }

    if(moveY == -1){ //100msに1回回転できる
      drawCurrentBlock(0);
      rotateBlock();  //回転
      drawCurrentBlock(1);
    }
    previousMillis = currentMillis;
  }
  
  drawCurrentBlock(1);

  draw();

  if (nextFlag == 1) {
    drawCurrentBlock(1); // 一旦消したブロックを、最終位置に書き戻し固定する
    judgeLine();         // そろったラインを消す
    judgeGameOver();     // ゲームオーバーの判定
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
  
  for(x = 0; x < shapes[currentShapeType].size; x++){
    for(y = 0; y < shapes[currentShapeType].size; y++){ 
      if(currentShape[x][y] == 1){
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

  for(x = 0; x < shapes[currentShapeType].size; x++){
    for(y = 0; y < shapes[currentShapeType].size; y++){
      if(currentShape[x][y] == 1){
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

  for(x = 0; x < shapes[currentShapeType].size; x++){
    for(y = 0; y < shapes[currentShapeType].size; y++){
      if(currentShape[x][y] == 1){
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
  if(bagIndex >= 7){
    for(int i = 0; i < 7; i++){ // ランダムな場所を選んで中身を入れ替えるのを7回やる
      int r = random(7);
      int temp = blockBag[i];
      blockBag[i] = blockBag[r];
      blockBag[r] = temp;
    }
    bagIndex = 0; // 0に戻す
  }

  currentShapeType = blockBag[bagIndex];
  bagIndex++; // 次はblockBagの次のブロックを取り出す

  for(int x = 0; x < maxBlockSize; x++){
    for(int y = 0; y < maxBlockSize; y++){
      currentShape[x][y] = shapes[currentShapeType].shape[x][y];  //図形の形をもらってくる
    }
  }

  currentX = 3;
  currentY = 0;
}

/*左右どちらに動かすか決定する*/
int wayOfMove(int stick){
  if(stick < 300){
    return -1;
  }else if(stick > 724){
    return 1;
  }else{
    return 0;
  }
}

//ブロックの回転を行う
void rotateBlock(){
  int temp[maxBlockSize][maxBlockSize] = {0};
  int blockSize;
  int rotateAbleFlag = 1; // =1:rotakeOK, =0:rotateNOK

  if(currentShapeType == 0){  //Oミノのとき 回転の必要なくね
    blockSize = 2;
  }else if(currentShapeType == 1){ //Iミノのとき
    blockSize = 4;
  }else{
    blockSize = 3;
  }

  for(int x = 0; x < blockSize; x++){
    for(int y = 0; y < blockSize; y++){
      temp[x][y] = currentShape[y][blockSize - 1 -x]; //90度回転
    }
  }

  //回転後空間に空きがあるか確かめる
  for(int x = 0; x < blockSize; x++){
    for(int y = 0; y < blockSize; y++){
      if(temp[x][y] == 1){
        if(block[currentX + x][currentY + y] == 1 || currentX + x < 0 || currentX + x >= X || currentY + y < 0 || currentY + y >= Y){  //回転した先にすでにブロックがあるか場外なら回転不可
          rotateAbleFlag = 0;
          break;
        }
      }
    }
    if(rotateAbleFlag == 0){
      break;
    }
  }

  if(rotateAbleFlag == 1){
    for(int x = 0; x < blockSize; x++){
      for(int y = 0; y < blockSize; y++){
        currentShape[x][y] = temp[x][y];
      }
    }
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

void judgeGameOver(){ //ミノが確定したときにミノが配列blockのy=0,1の領域に存在すれば終わり
  for(int x = 0; x < 8; x++){
    if(block[x][1] == 1){
      gameOver();
      break;
      //この後どうしよう
    }
  }
}

void gameOver(){
  while(1){ //チカチカ点滅の無限ループ
    //点灯処理
    draw(); //ブロックがあるところは光る
    delay(100);

    //消灯処理
    matrix.clear(); // 内部の描画メモリをクリア
    matrix.writeDisplay(); //画面を真っ暗にする
    delay(100);
  }
}