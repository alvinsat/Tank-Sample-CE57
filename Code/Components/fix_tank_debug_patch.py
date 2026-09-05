from pathlib import Path
path = Path(__file__).with_name('TankComponent.cpp')
text = path.read_text(encoding='utf-8')
old = '''\t\t\t\tif (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
\t\t\t\t{
\t\t\t\t\tpe_status_dynamics dynCheck;
\t\t\t\t\tbool gotDyn = false;
\t\t\t\t\tif (pPhysics)
\t\t\t\t\t\tgotDyn = (pPhysics->GetStatus(&dynCheck) != 0);

\t\t\t\t\tCryLogAlways("[Tank][Diag] ProcessMovementAdvanced: pPhysics=%p gotDyn=%d mass=%.3f vel=%.3f wheelCount=%d wheelPhysParts=%d",
\t\t\t\t\t\tpPhysics, gotDyn ? 1 : 0, gotDyn ? dynCheck.mass : 0.0f, gotDyn ? dynCheck.v.len() : 0.0f,
\t\t\t\t\t\tm_wheelCount, static_cast<int>(m_wheelPhysPartIds.size()));
\t\t\t\t}

\t\t\t\tif (!pPhysics || !pPhysics->GetStatus(&m_vehicleStatus))
\t\t\t\t\treturn;
\t\t\t{
\t\t\t\tCryLogAlways("[Tank Diagnostics] Wheel 0 -> Suspension Len: %.3f | Contact Normal Z: %.3f | Torque Angular Vel: %.2f",
\t\t\t\t\t firstWheelStatus.suspLen,
\t\t\t\t\t firstWheelStatus.normContact, // Ground surface normal vector pointing up
\t\t\t\t\t firstWheelStatus.w\t\t   // Actual rotational spin speed of the wheel proxy
\t\t\t\t);
\t\t\t}
\t\t}
\t}
\t//
'''
new = '''\t\t}\n\t}\n\t//\n'''
if old not in text:
    print('Old block not found')
    import sys
    sys.exit(1)
path.write_text(text.replace(old, new, 1), encoding='utf-8')
print('patched')
