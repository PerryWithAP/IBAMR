# Scripts to generate paraview visuals with IBAMR outputs

## How Create Scripts

*Step 1* - Open paraview

*Step 2* - Click "Tools" on the top, and then "Start Trace" and "OK"

*Step 3* - Do the task you want to be replicated with a script

*Step 4* - Go back to the same place you found "Start Trace" and click "Stop Trace"

*Step 5* - The Python Script Editor that appears will have the full script, copy it

 Be careful of residual parts of the script that might cause errors. For example, if you input an incorrect function into the calculator and then press apply, go back and fix the mistake, and then apply again, be sure to remove the instance where you inputed something incorrect from the resulting trace. There might also be instances where the trace might include another postioning call after you have already taken a screenshot, saved an animation, etc. these are arbitrary function calls and they are okay to remove.

## why is creating traces more efficient than coding from scratch 

This system is intended to mitigate the hastle of documentation referencing. Creating a trace makes sure that errors coming from the ordering/presence of function calls don't occur and effeciently makes a task reproducable.

## What was changed in the examples below and why 

In some of the examples, we use use python loops to reiterate on function calls to save screenshots and then do a affine translation. This is because static 3D models in paraview don't directly translate to animations througn paraview. All scripts require FFmpeg post processing and output a series of pngs. This is because Paraview does not output directly to mp4. Lastly, the outputs for all scripts were changed to output into the working/home directory.

## Rotation Animation in 3D On Sliced Object Static Model

### Using paraview version 6.0.1

The python script below called "RotationTrace.py" will first slice an object and take 250 screenshots of 'fch_ep_bb_as_v6_fibers_cap.e' as the camera rotates around the 3D object. Be sure to change FileName and registrationName to your respective file name and file directory. Some import considerations is the original focal point which will need to be the center of the 3D model, the original camera position which will need to reflect the focal point, the original camera view up which will be orthogonal to the vector between the focal point and camera position, and then the slice plane which will be perpendicular to the plane of rotation. 

```diff


from paraview.simple import *

paraview.simple._DisableFirstRenderCameraReset()

fch_ep_bb_as_v6_fibers_cape = IOSSReader(registrationName='fch_ep_bb_as_v6_fibers_cap.e', FileName=['/proj/griffithlab/perry/fch_ep_bb_as_v6_fibers_cap.e'])

animationScene1 = GetAnimationScene()

animationScene1.UpdateAnimationUsingDataTimeSteps()

renderView1 = GetActiveViewOrCreate('RenderView')

fch_ep_bb_as_v6_fibers_capeDisplay = Show(fch_ep_bb_as_v6_fibers_cape, renderView1, 'UnstructuredGridRepresentation')

fch_ep_bb_as_v6_fibers_capeDisplay.Representation = 'Surface'

renderView1.ResetCamera(False, 0.9)

materialLibrary1 = GetMaterialLibrary()

renderView1.Update()

ColorBy(fch_ep_bb_as_v6_fibers_capeDisplay, ('FIELD', 'vtkBlockColors'))

fch_ep_bb_as_v6_fibers_capeDisplay.SetScalarBarVisibility(renderView1, True)

vtkBlockColorsLUT = GetColorTransferFunction('vtkBlockColors')

vtkBlockColorsPWF = GetOpacityTransferFunction('vtkBlockColors')

vtkBlockColorsTF2D = GetTransferFunction2D('vtkBlockColors')

clip1 = Clip(registrationName='Clip1', Input=fch_ep_bb_as_v6_fibers_cape)

clip1.ClipType.Normal = [0.0, -1.0, 0.0]

clip1Display = Show(clip1, renderView1, 'UnstructuredGridRepresentation')

clip1Display.Representation = 'Surface'

Hide(fch_ep_bb_as_v6_fibers_cape, renderView1)

renderView1.Update()

ColorBy(clip1Display, ('FIELD', 'vtkBlockColors'))

clip1Display.SetScalarBarVisibility(renderView1, True)

layout1 = GetLayout()

layout1.SetSize(1029, 462)

+ import numpy as np

+ for i in range(0,250): 
+   OriginalCameraPosition = np.array([102.55216946684871, -470.18384082193745, 167.56612975705426])

+    OriginalCameraFocalPoint = np.array([100.00341331964601, 85.2722682952883, 102.8555135726927])

+    OriginalCameraViewUp= np.array([0, 0, 1])

+    Original_theta = np.arctan2(OriginalCameraPosition[1],OriginalCameraPosition[0])

+    ViewingDirection = OriginalCameraFocalPoint - OriginalCameraPosition

+    NormalViewingDirection = ViewingDirection / np.sqrt((ViewingDirection @ ViewingDirection))

+    TransformationMatrix = np.eye(3)

+    New_theta = Original_theta + i * .1

+   TransformationMatrix[0,0] = np.cos(New_theta) 
+   TransformationMatrix[0,1] = -np.sin(New_theta) 
+   TransformationMatrix[1,1] = np.cos(New_theta) 
+   TransformationMatrix[1,0] = np.sin(New_theta)
+   NewCameraViewUp = TransformationMatrix @ OriginalCameraViewUp 
+   NewCameraPosition = TransformationMatrix @ ViewingDirection + OriginalCameraFocalPoint 

    
    renderView1.Set(
        CameraPosition=NewCameraPosition.tolist(),
        CameraFocalPoint=[100.00341331964601, 85.2722682952883, 102.8555135726927],
        CameraViewUp=NewCameraViewUp.tolist(),
        CameraParallelScale=144.73642557699276,
    )

    SaveScreenshot(filename=f'SlicedPic{i}.png', viewOrLayout=renderView1, location=16, ImageResolution=[936, 382])
```


**Changed**

```
renderView1.Set(
    CameraPosition=NewCameraPosition.tolist(),
    CameraFocalPoint=[100.00341331964601, 85.2722682952883, 102.8555135726927],
    CameraViewUp=NewCameraViewUp.tolist(),
    CameraParallelScale=144.73642557699276,
)

SaveScreenshot(filename=f'SlicedPic{i}.png', viewOrLayout=renderView1, location=16, ImageResolution=[936, 382])
```

**Added**

```
import numpy as np

for i in range(0,250): 
    OriginalCameraPosition = np.array([102.55216946684871, -470.18384082193745, 167.56612975705426])

    OriginalCameraFocalPoint = np.array([100.00341331964601, 85.2722682952883, 102.8555135726927])

    OriginalCameraViewUp= np.array([0, 0, 1])

    Original_theta = np.arctan2(OriginalCameraPosition[1],OriginalCameraPosition[0])

    ViewingDirection = OriginalCameraFocalPoint - OriginalCameraPosition

    NormalViewingDirection = ViewingDirection / np.sqrt((ViewingDirection @ ViewingDirection))

    TransformationMatrix = np.eye(3)

    New_theta = Original_theta + i * .1

    TransformationMatrix[0,0] = np.cos(New_theta) 
    TransformationMatrix[0,1] = -np.sin(New_theta) 
    TransformationMatrix[1,1] = np.cos(New_theta) 
    TransformationMatrix[1,0] = np.sin(New_theta)
    NewCameraViewUp = TransformationMatrix @ OriginalCameraViewUp 
    NewCameraPosition = TransformationMatrix @ ViewingDirection + OriginalCameraFocalPoint 
```

Below is a shell script that uses the executable  "pvpython" to run the "RotationTrace.py"

```
#!/bin/bash

#SBATCH --job-name="Paraview-heart model"
#SBATCH --partition=general
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --time=00:20:00
#SBATCH --hint=nomultithread
#SBATCH --mem=15G

set -euo pipefail

/nas/longleaf/home/psor1241/Applications/ParaView-6.0.1-MPI-Linux-Python3.12-x86_64/bin/pvpython RotationTrace.py
```

Below is a shell script that uses the module ffmpeg to encode all the SlicedPic.png into an mp4 called "output.mp4"

```
#!/bin/bash

#SBATCH --job-name="Paraview-heart model"
#SBATCH --partition=general
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --time=00:20:00
#SBATCH --hint=nomultithread
#SBATCH --mem=15G

set -euo pipefail

ffmpeg -framerate 30 -i SlicedPic%d.png -c:v libx264 -r 30 -pix_fmt yuv420p output.mp4
```

## Rotation Animation in 3D On Time Series (One Rotation)

### Using paraview version 6.0.1

The python script below called "RotationTraceAnimation.py" will first load 'SurfaceMeter_Mitral_Valve.ex-timeseries' and the camera rotates once around the 3D object and then continues to load the rest of the timeseries. Then the result is made into an mp4 animation. Be sure to change FileName and registrationName to your respective file name and file directory. Some import considerations is the original focal point which will need to be the center of the 3D model, the original camera position which will need to reflect the focal point, and the original camera view up which will be orthogonal to the vector between the focal point and camera position.

```diff

from paraview.simple import *
paraview.simple._DisableFirstRenderCameraReset()

surfaceMeter_Mitral_Valveextimeseries = IOSSReader(registrationName='SurfaceMeter_Mitral_Valve.ex-timeseries', FileName=['/proj/griffithlab/perry/SurfaceMeter_Mitral_Valve.ex-timeseries'])

animationScene1 = GetAnimationScene()

animationScene1.UpdateAnimationUsingDataTimeSteps()

renderView1 = GetActiveViewOrCreate('RenderView')

surfaceMeter_Mitral_ValveextimeseriesDisplay = Show(surfaceMeter_Mitral_Valveextimeseries, renderView1, 'UnstructuredGridRepresentation')

surfaceMeter_Mitral_ValveextimeseriesDisplay.Representation = 'Surface'

renderView1.ResetCamera(False, 0.9)

materialLibrary1 = GetMaterialLibrary()

renderView1.Update()

renderView1.ResetActiveCameraToPositiveX()

renderView1.ResetCamera(False, 0.9)

cameraAnimationCue1 = GetCameraTrack(view=renderView1)

keyFrame14587 = CameraKeyFrame()
keyFrame14587.Set(
    PositionPathPoints=[0.9512036949993599, 1.8691603348403352, 9.26351822585893, 6.57631319594373, 1.8691603348403352, 7.435809355535248, 10.052822057967308, 1.8691603348403352, 2.6508054114881796, 10.052822057967308, 1.8691603348403352, -3.2637847359585965, 6.57631319594373, 1.8691603348403352, -8.048788680005664, 0.9512036949993616, 1.8691603348403352, -9.876497550329344, -4.673905805945005, 1.8691603348403352, -8.048788680005664, -8.150414667968583, 1.8691603348403352, -3.263784735958599, -8.150414667968583, 1.8691603348403352, 2.6508054114881734, -4.673905805945008, 1.8691603348403352, 7.435809355535238],
    FocalPathPoints=[0.9512036949993599, 1.8691603348403352, -0.30648966223520957],
    ClosedPositionPath=1,
)

keyFrame14588 = CameraKeyFrame()
keyFrame14588.KeyTime = .3

cameraAnimationCue1.Set(
    Mode='Path-based',
    KeyFrames=[keyFrame14587, keyFrame14588],
)

keyFrame14587.Set(
    Position=[-17.7306, 1.95407, -0.106344],
    ViewUp=[0.0, 0.0, 1.0],
    PositionPathPoints=[-17.7306, 1.95407, -0.106344, -10.763095585500055, -12.683921997421344, -0.106344, 5.025515345045449, -16.36314642201227, -0.106344, 17.74608673196641, -6.313072239554951, -0.106344, 17.81976886854336, 9.898389706407755, -0.106344, 5.191077603092175, 20.06367791700781, -0.106344, -10.630324963147327, 16.528123035573, -0.106344],
)

animationScene1.Play()

layout1 = GetLayout()

layout1.SetSize(990, 318)

renderView1.Set(
    CameraPosition=[-17.730600000000003, 1.9540699999999944, -0.106344],
    CameraFocalPoint=[0.9512036949993599, 1.8691603348403352, -0.30648966223520957],
    CameraViewUp=[0.0, 0.0, 1.0],
    CameraParallelScale=1.0,
)

SaveAnimation(filename='frame.png', viewOrLayout=renderView1, location=16, ImageResolution=[990, 318],
    FrameWindow=[0, 315])
```

Below is a shell script that uses the executable "pvpython" to run "RotationTraceAnimation.py"

```
#!/bin/bash

#SBATCH --job-name="Paraview-heart model"
#SBATCH --partition=general
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --time=00:20:00
#SBATCH --hint=nomultithread
#SBATCH --mem=15G

set -euo pipefail

/nas/longleaf/home/psor1241/Applications/ParaView-6.0.1-MPI-Linux-Python3.12-x86_64/bin/pvpython RotationTraceAnimation.py
```

Below is a shell script that uses the module ffmpeg to encode all the frame.####.png into an mp4 called "output.mp4"

```
#!/bin/bash

#SBATCH --job-name="Paraview-heart model"
#SBATCH --partition=general
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --time=00:20:00
#SBATCH --hint=nomultithread
#SBATCH --mem=15G

set -euo pipefail

ffmpeg -framerate 30 -i frame.%04d.png -c:v libx264 -r 30 -pix_fmt yuv420p output.mp4
```

## Animation in 2D On Lid Driven Capacity Vorticity

### Using paraview version 6.0.1

The python script below called "LidDrivenCapacity.py" will load the surface plots, visualize the vorticity, and then log scale the vorticity. Then the result is made into an mp4 animation. Be sure to change FileName and registrationName to your respective file name and file directory. 

```diff

from paraview.simple import *

paraview.simple._DisableFirstRenderCameraReset()


dumpssamraiseries = VisItSAMRAIReader(registrationName='dumps.samrai.series', FileName=['/proj/griffithlab/perry/lid-driven-cavity/dumps.samrai.series'])

animationScene1 = GetAnimationScene()

animationScene1.UpdateAnimationUsingDataTimeSteps()

dumpssamraiseries.Set(
    CellArrayStatus=['F'],
    PointArrayStatus=['U'],
)

renderView1 = GetActiveViewOrCreate('RenderView')

dumpssamraiseriesDisplay = Show(dumpssamraiseries, renderView1, 'AMRRepresentation')

dumpssamraiseriesDisplay.Representation = 'Outline'

renderView1.ResetCamera(False, 0.9)

materialLibrary1 = GetMaterialLibrary()

renderView1.Update()

dumpssamraiseriesDisplay.SetRepresentationType('Surface')

ColorBy(dumpssamraiseriesDisplay, ('POINTS', 'U', 'Magnitude'))

dumpssamraiseriesDisplay.RescaleTransferFunctionToDataRange(True, False)

dumpssamraiseriesDisplay.SetScalarBarVisibility(renderView1, True)

uLUT = GetColorTransferFunction('U')

uPWF = GetOpacityTransferFunction('U')

uTF2D = GetTransferFunction2D('U')

ColorBy(dumpssamraiseriesDisplay, None)

HideScalarBarIfNotNeeded(uLUT, renderView1)

dumpssamraiseries.PointArrayStatus = ['Omega']

renderView1.Update()

ColorBy(dumpssamraiseriesDisplay, ('POINTS', 'Omega'))

dumpssamraiseriesDisplay.RescaleTransferFunctionToDataRange(True, False)

dumpssamraiseriesDisplay.SetScalarBarVisibility(renderView1, True)

omegaLUT = GetColorTransferFunction('Omega')

omegaPWF = GetOpacityTransferFunction('Omega')

omegaTF2D = GetTransferFunction2D('Omega')

ColorBy(dumpssamraiseriesDisplay, None)


HideScalarBarIfNotNeeded(omegaLUT, renderView1)

ColorBy(dumpssamraiseriesDisplay, ('POINTS', 'Omega'))

dumpssamraiseriesDisplay.RescaleTransferFunctionToDataRange(True, False)

dumpssamraiseriesDisplay.SetScalarBarVisibility(renderView1, True)

calculator1 = Calculator(registrationName='Calculator1', Input=dumpssamraiseries)

calculator1.Function = 'sign(Omega) * log10(abs(Omega) + 1)'

calculator1Display = Show(calculator1, renderView1, 'AMRRepresentation')

calculator1Display.Representation = 'Outline'

Hide(dumpssamraiseries, renderView1)

calculator1Display.SetScalarBarVisibility(renderView1, True)

renderView1.Update()

calculator1.ResultArrayName = 'Log scaled'

renderView1.Update()

calculator1Display.SetRepresentationType('Surface')

calculator1.Function = 'sign(Omega) * log10(abs(Omega) + 1)'

renderView1.Update()

ColorBy(calculator1Display, ('POINTS', 'Log scaled'))

HideScalarBarIfNotNeeded(omegaLUT, renderView1)

calculator1Display.RescaleTransferFunctionToDataRange(True, False)

calculator1Display.SetScalarBarVisibility(renderView1, True)

logscaledLUT = GetColorTransferFunction('Logscaled')

logscaledPWF = GetOpacityTransferFunction('Logscaled')

logscaledTF2D = GetTransferFunction2D('Logscaled')

logscaledLUT.RescaleTransferFunction(-0.5, 0.5)

logscaledPWF.RescaleTransferFunction(-0.5, 0.5)

logscaledTF2D.RescaleTransferFunction(-0.5, 0.5, 0.0, 1.0)

logscaledLUT.ApplyPreset('Cool to Warm', True)

partpvtuseries = XMLPartitionedUnstructuredGridReader(registrationName='Part.pvtu.series', FileName=['/proj/griffithlab/perry/lid-driven-cavity/Part.pvtu.series'])

partpvtuseries.TimeArray = 'None'

partpvtuseriesDisplay = Show(partpvtuseries, renderView1, 'UnstructuredGridRepresentation')

partpvtuseriesDisplay.Representation = 'Surface'

renderView1.Update()

layout1 = GetLayout()

layout1.SetSize(1052, 474)

renderView1.Set(
    InteractionMode='2D',
    CameraPosition=[0.5, 0.5, 2.7320508075688776],
    CameraFocalPoint=[0.5, 0.5, 0.0],
    CameraParallelScale=0.7071067811865476,
)

SaveAnimation(filename='Lidframe.png', viewOrLayout=renderView1, location=2, ImageResolution=[1053, 474],
    FrameWindow=[0, 100])
```

Below is a shell script that uses the executable "pvpython" to run "LidDrivenCapacity.py"

```
#!/bin/bash

#SBATCH --job-name="Paraview-heart model"
#SBATCH --partition=general
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --time=00:20:00
#SBATCH --hint=nomultithread
#SBATCH --mem=15G

set -euo pipefail

/nas/longleaf/home/psor1241/Applications/ParaView-6.0.1-MPI-Linux-Python3.12-x86_64/bin/pvpython RotationTraceAnimation.py
```

Below is a shell script that uses the module ffmpeg to encode all the Lidframe.####.png into an mp4 called "output.mp4"

```
#!/bin/bash

#SBATCH --job-name="Paraview-heart model"
#SBATCH --partition=general
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --time=00:20:00
#SBATCH --hint=nomultithread
#SBATCH --mem=15G

set -euo pipefail

ffmpeg -framerate 30 -i Lidframe%04d.png -c:v libx264 -r 30 -pix_fmt yuv420p output.mp4
```