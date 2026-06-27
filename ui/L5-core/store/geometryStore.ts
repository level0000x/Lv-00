import { create } from 'zustand';

export type GeoObjectType = 'point' | 'segment' | 'line' | 'ray' | 'circle' | 'arc' | 'polygon' | 'angle';