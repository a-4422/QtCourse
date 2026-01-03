@echo off
REM Generate Model/View diagram PNG from PlantUML source
if exist "%~dp0\plantuml.jar" (
  java -jar "%~dp0\plantuml.jar" -tpng model_view.puml
) else (
  echo plantuml.jar not found in project root.
  echo Please place plantuml.jar here or install PlantUML globally and run:
  echo     plantuml -tpng model_view.puml
)



