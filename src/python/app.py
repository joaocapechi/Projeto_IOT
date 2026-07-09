from flask import Flask, send_file, make_response
import os
import time

app = Flask(__name__)

IMAGE_PATH = os.path.abspath("detected_person.jpg")


@app.route("/camera.jpeg")
def camera_image():
    """
    Returns the latest detected_person.jpg image.
    The URL ends with .jpeg so Grafana can load it.
    """
    print(IMAGE_PATH)

    if not os.path.exists(IMAGE_PATH):
        return "Image not found", 404

    response = make_response(
        send_file(
            IMAGE_PATH,
            mimetype="image/jpeg"
        )
    )

    # Prevent browser/Grafana caching old images
    response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
    response.headers["Pragma"] = "no-cache"
    response.headers["Expires"] = "0"

    return response


if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=5000,
        debug=False
    )
