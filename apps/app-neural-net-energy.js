#!/usr/bin/env chemwiz

//
// app-neural-net-energy.js is a research application which purpose is to aid in research on
// computation of energy of atomic configurations.
// It allows to:
// 1. Generate and keep track of many test configurations
// 2. Generate a set of neural networks that allow to compute the enery very fast
// 3. Compute enery and dynamics of complex molecular systems
//

var helpers = {
	getenvz: function(name, deft) { // returns an environment variable for a supplied name, or the default value when the enironment variable isn't set
		return getenv(name)==null ? deft : getenv(name);
	},
	getenvlz: function(name, deft) { // returns a colon-separated list from the environment variable for a supplied name, or the default value when the enironment variable isn't set
		return this.getenvz(name, deft).split(':');
	},
	requirenAndCreate: function(lst) {
		var mm = [];
		lst.forEach(function(name) {
			mm.push(require(name).create());
		});
		return mm;
	}
};

/// computation engine that we use

var Engines = helpers.requirenAndCreate(helpers.getenvlz("ENGINES", "calc-nwchem:calc-erkale"));
var cparams = {dir:"/tmp", precision: "0.01"}

/// functions

function usage() {
	throw "Usage: app-neural-net-based-energy-computation.js {db|generate|compute|export} ...\n";
}

function calcEnergy(engine, params, molecule) {
	var time1 = new Date().getTime();
	var energy = engine.calcEnergy(molecule, params)
	var time2 = new Date().getTime();
	return {engine: engine.kind(), energy: energy, precision: params.precision, elapsed: time2-time1, timestamp: time2};
}

var actions = {
	db: {
		// internals
		_dbFileName_: helpers.getenvz("DB", "nn-energy-computations.sqlite"),
		open: function() {
			return require('sqlite3').openDatabase(this._dbFileName_);
		},
		// db ... commands
		create: function() {
			//File.unlink(this._dbFileName_);
			var db = this.open();
			db.run("CREATE TABLE energy(id INTEGER PRIMARY KEY AUTOINCREMENT, energy REAL, precision REAL, elapsed INTEGER, timestamp INTEGER, engine TEXT);");
			db.run("CREATE TABLE xyz(energy_id INTEGER, elt TEXT, x REAL, y REAL, z REAL, FOREIGN KEY(energy_id) REFERENCES energy(id));");
			db.run("CREATE INDEX index_xyz_energy_id ON xyz(energy_id);");
			db.run("CREATE VIEW energy_view AS SELECT e.*"+
				", (SELECT COUNT(*) FROM xyz WHERE xyz.energy_id = e.id) AS num_atoms"+
				", (SELECT GROUP_CONCAT(xyz.elt) FROM xyz WHERE xyz.energy_id = e.id) AS formula"+
				" FROM energy e;");
			db.close();
		},
		stats: function() {
		},
		// internally-used commands
		insertEnergy: function(db, energy, precision, elapsed, timestamp, engine, molecule) {
			var db = this.open();
			db.run("BEGIN TRANSACTION")
			db.run("INSERT INTO energy(energy,precision,elapsed,timestamp,engine) VALUES ("+energy+", "+precision+", "+elapsed+", "+timestamp+", '"+engine+"');")
			db.run("SELECT MAX(id) FROM energy;", function(opaque,nCols,fldValues,fldNamesValues) {
				var energy_id = fldValues[0];
				for (var a = 0; a < molecule.numAtoms(); a++) {
					var atom = molecule.getAtom(a);
					var atomPos = atom.getPos();
					db.run("INSERT INTO xyz(energy_id,elt,x,y,z) VALUES ("+energy_id+", '"+atom.getElement()+"', "+atomPos[0]+", "+atomPos[1]+", "+atomPos[2]+");")
				}
			});
			db.run("COMMIT")
		}
	},
	generate: {
		// internals
		_fromMoleculeObject_: function(db, molecule) {
			Engines.forEach(function(engine) {
				try {
					var rec = calcEnergy(engine, cparams, molecule);
					actions.db.insertEnergy(db, rec.energy, rec.precision, rec.elapsed, rec.timestamp, rec.engine, molecule);
				} catch (err) {
					print("EXCEPTION("+engine.kind()+"): molecule failed: "+err);
					writeXyzFile(molecule, "FAILED-molecule-"+engine.kind()+".xyz");
				}
			});
		},
		_fromSmiles_: function(db, smi) {
			var molecule = Moleculex.fromSMILES(smi);
			Engines.forEach(function(engine) {
				try {
					var rec = calcEnergy(engine, cparams, molecule);
					actions.db.insertEnergy(db, rec.energy, rec.precision, rec.elapsed, rec.timestamp, rec.engine, molecule);
				} catch (err) {
					print("EXCEPTION("+engine.kind()+"): smi="+smi+" failed: "+err);
					writeXyzFile(molecule, "FAILED-"+engine.kind()+"-smi-"+smi+".xyz");
				}
			});
		},
		// generate ... commands
		fromSmilesText: function(smiText) {
			var db = actions.db.open();
			this._fromSmiles_(db, smiText);
			db.close();
		},
		fromSmilesFile: function(fileName) {
			var db = actions.db.open();
			var fromSmiles = this._fromSmiles_;
			File.read(fileName).split("\n").forEach(function(smi) {
				if (smi=="")
					return;
				fromSmiles(db, smi);
			});
			db.close();
		},
		randomConfigurationsOfElt: function(elt, numAtoms, numConfs, boxSize, minDist, maxDist) {
			print("randomConfigurationsOfElt: elt="+elt);
			var randCoord = function() {
        			var max = 1000000;
        			return LegacyRng.rand()%max/max * boxSize - boxSize/2;
			}
			var mm = [];
			while (mm.length < numConfs) {
				var molecule = new Molecule;
				for (var a = 0; a < numAtoms; a++) {
					var atom = new Atom(elt, [randCoord(), randCoord(), randCoord()]);
					molecule.addAtom(atom);
				}
				if (minDist <= molecule.minDistBetweenAtoms() && molecule.maxDistBetweenAtoms() <= maxDist) {
					print("adding the configuration with dists in "+molecule.minDistBetweenAtoms()+".."+molecule.maxDistBetweenAtoms());
					mm.push(molecule);
				}
			}
			var fromMoleculeObject = this._fromMoleculeObject_;
			var db = actions.db.open();
			mm.forEach(function(molecule) {
				fromMoleculeObject(db, molecule);
			});
			db.close();
		}
	},
	export: {
		energyAsXyz: function(energyId, fileName) {
			var db = actions.db.open();
			var xyzData = "";
			var numAtoms = 0;
			db.run("SELECT elt,x,y,z FROM xyz WHERE energy_id="+energyId+";", function(opaque,nCols,fldValues,fldNamesValues) {
				var energy_id = fldValues[0];
				xyzData = xyzData + fldValues[0] + " " + fldValues[1] + " " + fldValues[2] + " " + fldValues[3] + "\n";
				numAtoms++;
				return false; // continue
			});
			xyzData = "energy#"+energyId+" exported by app-neural-net-energy.js\n" + xyzData;
			xyzData = numAtoms+"\n" + xyzData;
			db.close();

			// write the file
			File.write(xyzData, fileName);
		}
	}
};


/// MAIN

function main(args) {
	if (args.length < 1)
		usage();

	if (args[0] == "db") {
		if (args.length < 2)
			usage();
		else if (args[1] == "create") {
			actions.db.create();
		} else if (args[1] == "stats")
			actions.db.stats();
		else
			usage();
	} else if (args[0] == "generate") {
		if (args.length >= 2 && args[1] == "from-smiles-text") {
			for (var i = 2; i < args.length; i++)
				actions.generate.fromSmilesText(args[i]);
		} else if (args.length >= 2 && args[1] == "from-smiles-file") {
			for (var i = 2; i < args.length; i++)
				actions.generate.fromSmilesFile(args[i]);
		} else if (args.length==8 && args[1] == "random-configurations-of-elt") {
			actions.generate.randomConfigurationsOfElt(
				args[2]/*elt*/,
				args[3]/*numAtoms*/,
				args[4]/*numConfs*/,
				args[5]/*box-size (Å)*/,
				args[6]/*min-dist (Å)*/,
				args[7]/*max-dist (Å)*/
			);
		} else
			usage();
	} else if (args[0] == "compute") {
	} else if (args[0] == "export") {
		if (args.length < 2) {
			usage();
		} else if (args.length==4 && args[1] == "energy-as-xyz") {
				actions.export.energyAsXyz(args[2], args[3]);
		} else
			usage();
	} else {
		usage();
	}
}

main(scriptArgs);
print("--END--");
