# -*- coding: utf-8 -*-
"""render_headless.py — one-shot headless Movie Render Queue render from a JSON state.

Flow:
  1. read the JSON state path / output dir / warm-up frames from environment
     variables set by render_headless.bat (or defaults)
  2. call unreal.MetaHumanRenderController.render_headless() which applies the state,
     resolves the camera, starts the render, and requests editor exit on completion.

Runs inside the editor (UnrealEditor-Cmd.exe, NOT -game) so the MRQ in-process
executor can advance on the editor frame loop.
"""

import os

import unreal


def _normalize(path):
    return path.replace("\\", "/")


def _resolve(path, default):
    if not path:
        path = default
    if not os.path.isabs(path):
        path = os.path.join(unreal.Paths.project_dir(), path)
    return _normalize(path)


def main():
    state_path = _resolve(os.environ.get("META_STATE"), "Saved/ToolOutput/exported_state.json")
    out_dir = _resolve(os.environ.get("META_OUT"), "Saved/ToolOutput")
    warmup = int(os.environ.get("META_WARMUP", "32"))

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        unreal.log_error("render_headless: no editor world available")
        unreal.SystemLibrary.quit_editor()
        return 1

    unreal.log("render_headless: state={} out={} warmup={}".format(state_path, out_dir, warmup))

    ok = unreal.MetaHumanRenderController.render_headless(world, state_path, out_dir, warmup)
    if not ok:
        unreal.log_error("render_headless: render failed to start")
        unreal.SystemLibrary.quit_editor()
        return 1

    # The PIE render advances asynchronously on the editor frame loop. By default the
    # -ExecutePythonScript runner issues QUIT_EDITOR on the next tick after this script
    # returns, which would kill the render during warm-up. Keep the editor alive instead;
    # the C++ side requests editor exit when the render actually finishes.
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)

    unreal.log("render_headless: render started; editor will exit when complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
