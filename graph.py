import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

before_data = pd.read_csv("/content/test1_before_exhaust (1).CSV")

add_val = int(before_data.at[63674,"Time_Milliseconds"])

after_data = pd.read_csv("/content/test1_after_exhaust (1).CSV")
after_data["Time_Milliseconds"] += add_val

full_data = pd.concat([before_data, after_data], axis=0)

x=full_data["Time_Milliseconds"]
y=full_data["Relative_Humidity_%"]

plt.figure(figsize=(60, 10))

plt.scatter(x, y)

plt.xlabel("Time in Milliseconds")
plt.ylabel("Relative Humidity %")
