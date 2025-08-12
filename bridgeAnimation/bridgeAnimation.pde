int spanPos;

void setup() {
  size(1000, 1000);
  background(255);
  noSmooth();
  frameRate(30);
}

void draw() {
  background(255);
  drawBridge(2);
}

void drawBridge(int state) {
  //state 0 = closed
  //stage 1 = open
  //state 2 = closing
  //state 3 = opening
  background(255);
  int stepSize = 3;
  if(state == 0) {
   spanPos = 0; 
  }
  if(state == 1) {
   spanPos = 100; 
  }
  if(state == 2) {
    if(spanPos > 0) {
      spanPos-= stepSize;
    } else {
     spanPos = 100; 
    }
  }
  if(state == 3) {
   if(spanPos < 100) {
     spanPos+= stepSize;
  } else {
   spanPos = 0; 
    }
  }
  fill(0);
  stroke(0);
  rect(200, 600, 200, 50);
  rect(600, 600, 200, 50);
  rect(380, 500, 20, 100);
  rect(600, 500, 20, 100);
  stroke(255, 0, 0);
  fill(255, 0, 0);
  rect(400, 600 - spanPos, 200, 10);
}
