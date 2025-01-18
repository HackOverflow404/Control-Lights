# LED Strip Controller with Flask and Raspberry Pi

This project replaces a lost IR remote for an LED light strip with a modern, smart solution. Using a Raspberry Pi, a few MOSFET transistors, and a Flask web application, the LED strip can now be controlled directly from a smartphone via a user-friendly web interface.

## Features

- **Smart LED Control**: Adjust the brightness and color of your LED strip via your phone.
- **Flask Web Interface**: A lightweight web server hosted on the Raspberry Pi allows for real-time LED control.
- **Simple Circuit Design**: Utilizes MOSFET transistors for reliable control of the LED strip.

## Requirements

### Hardware

- Raspberry Pi (any model with GPIO pins and Wi-Fi capabilities)
- LED light strip (RGB or single color)
- MOSFET transistors (e.g., IRF540N or similar)
- Resistors (to protect the GPIO pins)
- Power supply compatible with the LED strip
- Breadboard and jumper wires (for prototyping)

### Software

- Python 3
- Flask
- Raspberry Pi OS (Lite or Desktop)

## Circuit Design

The circuit connects the LED strip to the Raspberry Pi through MOSFET transistors:

1. The **positive terminal** of the LED strip connects to the power supply.
2. The **negative terminal** (or control terminals for RGB) connects to the drain of each MOSFET.
3. The **source** of each MOSFET connects to the ground.
4. The **gate** of each MOSFET connects to a GPIO pin on the Raspberry Pi via a resistor.

Ensure proper connections and double-check the specifications of your MOSFETs to avoid overheating or damage.

## Installation

### 1. Set Up the Raspberry Pi

1. Flash Raspberry Pi OS onto an SD card.
2. Connect the Raspberry Pi to Wi-Fi and enable SSH.
3. Update the Raspberry Pi:
   ```bash
   sudo apt update && sudo apt upgrade -y
   ```

### 2. Install Dependencies

1. Install Python and pip:
   ```bash
   sudo apt install python3 python3-pip -y
   ```
2. Install Flask:
   ```bash
   pip3 install flask
   ```

### 3. Clone the Repository

Clone this project to your Raspberry Pi:

```bash
git clone https://github.com/your-username/led-strip-controller.git
cd led-strip-controller
```

### 4. Run the Application

1. Start the Flask server:
   ```bash

   python3 app.py
   ```
2. Open a browser on your phone or computer and navigate to:
   ```
   http://<raspberry-pi-ip>:5000
   ```

## Usage

1. Access the web interface using your phone or any device connected to the same network.
2. Use the sliders and buttons to adjust the LED strip’s color and brightness.

## Customization

- **GPIO Pin Configuration**: Modify the `config.py` file to change which GPIO pins control the LED channels.
- **UI**: Customize the HTML/CSS files in the `templates` and `static` folders to enhance the web interface.

## Future Enhancements

- Add support for animations and effects (e.g., breathing, fading).
- Integrate with smart home systems like Alexa, Google Home, or Home Assistant.
- Separating the functionality of the LED into an ESP32 to use as an MQTT client and the Raspberry Pi as a MQTT Broker

## Contributing

Contributions are welcome! Please submit issues and pull requests on the [GitHub repository](https://github.com/your-username/led-strip-controller).

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

---

Feel free to share feedback or suggest new features. Enjoy your smart LED strip controller!
