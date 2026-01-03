@echo off
REM Generate ER diagram PNG from PlantUML source (requires plantuml installed)
REM Usage: double-click or run in project root where plantuml.jar is available.

if exist "%~dp0\plantuml.jar" (
  java -jar "%~dp0\plantuml.jar" -tpng er_diagram.puml
) else (
  echo plantuml.jar not found in project root.
  echo Please install PlantUML or place plantuml.jar next to this script.
  echo You can also install plantuml via package manager and run:
  echo    plantuml -tpng er_diagram.puml
)



