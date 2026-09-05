-- CustomAbrams entity class derived from Abrams.
-- This file allows the engine to recognize a new vehicle class that can use CustomAbrams.xml.

Script.ReloadScript("Scripts/Entities/Vehicles/Implementations/Abrams.lua")

CustomAbrams = {}
MakeDerivedEntity(CustomAbrams, Abrams)
