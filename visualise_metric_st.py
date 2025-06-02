import streamlit as st
import argparse
import numpy as np
import os
import plotly.graph_objects as go

def visualise_depth_map(dp_path):
    st.title ("Depth Map Viewer")

    depth_files = [f for f in os.listdir(dp_path) if f.endswith(".npy")]

    selected_file = st.selectbox("Select a depth map file", depth_files)

    depth_np = np.flipud(np.load(os.path.join(dp_path, selected_file)))

    fig = go.Figure(data=go.Heatmap(
        z=depth_np,
        zmin=0,
        zmax=6,
        colorscale='plasma',
        colorbar=dict(title="Depth"),
        hovertemplate='X: %{x}<br>Y: %{y}<br>Depth: %{z:.2f}<extra></extra>'
    ))

    fig.update_layout(
        title=f"Depth Map: {selected_file}",
        xaxis_title="X Pixel",
        yaxis_title="Y Pixel",
    )

    st.plotly_chart(fig, use_container_width=True)

def main():
    dp_path = st.text_input("Enter path to depth maps:", "./output_stereo/")

    if os.path.isdir(dp_path):
        visualise_depth_map(dp_path)
    else:
        st.error(f"Directory '{dp_path}' not found.")

if __name__  == "__main__":
    main()