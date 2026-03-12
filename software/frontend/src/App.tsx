import StatusBar from './components/StatusBar'
import ServoControl from './components/ServoControl'
import LEDControl from './components/LEDControl'
import PDControl from './components/PDControl'
import SensorGraph from './components/SensorGraph'
import CalibrationPanel from './components/CalibrationPanel'

export default function App() {
  return (
    <div className="app-layout">
      <StatusBar />
      <div className="app-main">
        <div className="left-panel">
          <LEDControl />
          <PDControl />
          <ServoControl />
          <CalibrationPanel />
        </div>
        <div className="right-panel">
          <SensorGraph />
        </div>
      </div>
    </div>
  )
}
