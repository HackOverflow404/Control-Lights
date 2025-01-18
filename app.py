from flask import Flask, request, jsonify, render_template, redirect, url_for
import json
import os
import RPi.GPIO as GPIO

app = Flask(__name__)

# GPIO Pin Definitions
BLUE_PIN = 13
RED_PIN = 19
GREEN_PIN = 21

red_pwm = None
green_pwm = None
blue_pwm = None

# Initialize GPIO
def initialize():
    global red_pwm, green_pwm, blue_pwm
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(BLUE_PIN, GPIO.OUT)
    GPIO.setup(RED_PIN, GPIO.OUT)
    GPIO.setup(GREEN_PIN, GPIO.OUT)
    red_pwm = GPIO.PWM(RED_PIN, 100)
    green_pwm = GPIO.PWM(GREEN_PIN, 100)
    blue_pwm = GPIO.PWM(BLUE_PIN, 100)
    red_pwm.start(0)
    green_pwm.start(0)
    blue_pwm.start(0)

# Send the PWM command
def set_color(red, green, blue):
    global red_pwm, green_pwm, blue_pwm
    red_duty = red * 100 / 255
    green_duty = green * 100 / 255
    blue_duty = blue * 100 / 255
    red_pwm.ChangeDutyCycle(red_duty)
    green_pwm.ChangeDutyCycle(green_duty)
    blue_pwm.ChangeDutyCycle(blue_duty)

# Flask Routes
@app.route("/", methods=["POST", "GET"])
def index():
    data = {
        "is_on": False,
    }
    if "is_on" in request.args:
        data["is_on"] = request.args.get("is_on")
    
    return render_template("index.html", data=data)

# Initialize GPIO Route
@app.route("/startup")
def startup():
    initialize() # Call the initialize method
    
    # Write to the JSON file that the lights are turned on and fetch the color and preset data
    file_path = os.path.join(app.root_path, "static", "assets", "data.json")
    with open(file_path, "r+") as json_file:
        data = json.load(json_file)
        data["isOn"] = True
        json_file.write(data)
    
    set_color(data["currColor"][0], data["currColor"][1], data["currColor"][2])
    
    # Redirect to the index page on complete
    return redirect(url_for("index", is_on=True))

# Set Color Route
@app.route("/control", methods=["POST"])
def control():
    data = request.get_json()

    # Write to the JSON file the current color
    file_path = os.path.join(app.root_path, "static", "assets", "data.json")
    with open(file_path, "r+") as json_file:
        json_data = json.load(json_file)
        json_data["currColor"] = [int(data["red"]), int(data["green"]), int(data["blue"])]
        json_file.write(json_data)
    
    set_color(int(data["red"]), int(data["green"]), int(data["blue"]))

    # Return success message
    return jsonify({"message": "Post request successful"}), 200

# Save the data in to the JSON file
@app.route("/save_data", methods=["POST"])
def save_data():
    data = request.get_json()
    
    # Store into JSON file
    print("Saving data: ", data)
    json_data = json.dumps(data, indent=4)
    file_path = os.path.join(app.root_path, "static", "assets", "data.json")
    with open(file_path, "w") as outfile:
        outfile.write(json_data)
    
    # Returns url to redirect to shutdown
    return jsonify({
        'redirect': True,
        'redirect_url': url_for('shutdown')
    })

# Clean up GPIO on exit
@app.route("/shutdown", methods=["GET"])
def shutdown():
    red_pwm.stop()
    green_pwm.stop()
    blue_pwm.stop()
    GPIO.cleanup()
    return redirect(url_for("index", is_on=False))

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5500)