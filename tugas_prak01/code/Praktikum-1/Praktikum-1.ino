const int buttonPin = 4;
const int ledPin = 5;

int buttonState = 0;
bool ledState = false;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);
}

void loop() {
  buttonState = digitalRead(buttonPin);

  if (buttonState == HIGH) {

    // Mengubah state LED dari OFF menjadi ON
    ledState = !ledState;

    // Menerapkan state LED ke output
    digitalWrite(ledPin, ledState);

    if (ledState == true) {
      Serial.println("Tombol ditekan -> LED ON");
    } else {
      Serial.println("Tombol ditekan -> LED OFF");
    }

    // Debounce untuk mencegah satu tekanan
    delay(200);

    // Menunggu sampai tombol dilepas
    while (digitalRead(buttonPin) == HIGH) {
      delay(10);
    }
  }
}