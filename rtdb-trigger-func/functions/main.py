# The Cloud Functions for Firebase SDK to create Cloud Functions and set up triggers.
from firebase_functions import db_fn

# The Firebase Admin SDK to access the Firebase Realtime Database.
from firebase_admin import initialize_app, db

from pycaret.classification import load_model, predict_model
import pandas as pd

loaded_model = load_model('driving-behavior-1')
app = initialize_app()


@db_fn.on_value_updated(
        reference="/realtime/{deviceId}",
        memory=512,
        region="asia-southeast1"
)
def onupdatefunctiondefault(event: db_fn.Event[db_fn.Change]):
    new_data_event = event.data.after

    ref = db.reference('realtime').child(event.params["deviceId"])

    accelerometer = new_data_event['accelerometer']
    gyro = new_data_event['gyro']
    prediction = predict_model(loaded_model, data=pd.DataFrame({
        'AccX': [accelerometer['x']],
        'AccY': [accelerometer['y']],
        'AccZ': [accelerometer['z']],
        'GyroX': [gyro['x']],
        'GyroY': [gyro['y']],
        'GyroZ': [gyro['z']]
    }))
    print('=========CLOUD FUNCTION START=========')
    print(f'DATA: \naccelerometer: {accelerometer} \ngyro: {gyro}')
    print(f'LABEL: {prediction.iloc[0]["prediction_label"]} - score: {prediction.iloc[0]["prediction_score"]}')
    print('=========CLOUD FUNCTION END=========')

    if prediction.iloc[0]["prediction_label"] == 1:
        return ref.update({"behavior": "ABNORMAL"})
    return ref.update({"behavior": "NORMAL"})